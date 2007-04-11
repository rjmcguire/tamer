#include "tame_driver.hh"
#include <sys/select.h>
#include <stdio.h>
#include <signal.h>

namespace tame {

driver driver::main;

_rendezvous_base *_rendezvous_base::unblocked;
_rendezvous_base *_rendezvous_base::unblocked_tail;

rendezvous<> rendezvous<>::dead;
_event_superbase _event_superbase::dead(&rendezvous<>::dead, 0);

// signal handling inspired by sfslite.
#define NSIGNALS 32
static int sig_pipe[2] = { -1, -1 };
static volatile unsigned sig_any_active;
static volatile unsigned sig_active[NSIGNALS];
static event<> sig_handlers[NSIGNALS];

driver::driver()
    : _t(0), _nt(0),
      _fd(0), _fdcap(0), _nfds(0),
      _asap(0), _nasap(0), _asapcap(0),
      _tcap(0), _tgroup(0), _tfree(0)
{
    expand_timers();
    FD_ZERO(&_readfds);
    FD_ZERO(&_writefds);
}

driver::~driver()
{
    // destroy all active timers
    for (int i = 0; i < _nt; i++)
	_t[i]->~ttimer();

    // free timer groups
    while (_tgroup) {
	ttimer_group *next = _tgroup->next;
	delete[] reinterpret_cast<unsigned char *>(_tgroup);
	_tgroup = next;
    }
    delete[] _t;

    // free file descriptors
    for (int i = 0; i < _fdcap; i++)
	_fd[i].~event();
    delete[] reinterpret_cast<unsigned char *>(_fd);

    // free asap
    for (int i = 0; i < _nasap; i++)
	_asap[i].~event();
    delete[] reinterpret_cast<unsigned char *>(_asap);
}

void driver::expand_timers()
{
    int ncap = (_tcap ? _tcap * 2 : 16);

    ttimer_group *ngroup = reinterpret_cast<ttimer_group *>(new unsigned char[sizeof(ttimer_group) + sizeof(ttimer) * (ncap - 1)]);
    ngroup->next = _tgroup;
    _tgroup = ngroup;
    for (int i = 0; i < ncap; i++) {
	ngroup->t[i].u.next = _tfree;
	_tfree = &ngroup->t[i];
    }

    ttimer **t = new ttimer *[ncap];
    memcpy(t, _t, sizeof(ttimer *) * _nt);
    delete[] _t;
    _t = t;
    _tcap = ncap;
}

void driver::timer_reheapify_from(int pos, ttimer *t, bool /*will_delete*/)
{
    // MUST be called with timer lock held
    int npos;
    while (pos > 0
	   && (npos = (pos-1) >> 1, timercmp(&_t[npos]->expiry, &t->expiry, >))) {
	_t[pos] = _t[npos];
	_t[npos]->u.schedpos = pos;
	pos = npos;
    }

    ttimer **tend = _t + _nt;
    while (1) {
	ttimer *smallest = t;
	ttimer **tp = _t + 2*pos + 1;
	if (tp < tend && !timercmp(&tp[0]->expiry, &smallest->expiry, >))
	    smallest = tp[0];
	if (tp + 1 < tend && !timercmp(&tp[1]->expiry, &smallest->expiry, >))
	    smallest = tp[1], tp++;

	smallest->u.schedpos = pos;
	_t[pos] = smallest;

	if (smallest == t)
	    break;

	pos = tp - _t;
    }
#if 0
    if (_t + 1 < tend || !will_delete)
	_timer_expiry = tbegin[0]->expiry;
    else
	_timer_expiry = Timestamp();
#endif
}

void driver::expand_fds(int fd)
{
    int ncap = (_fdcap ? _fdcap * 2 : 16);
    while (fd*2 >= ncap)
	ncap *= 2; // XXX integer overflow

    event<> *nfd = reinterpret_cast<event<> *>(new unsigned char[sizeof(event<>) * ncap]);
    memcpy(nfd, _fd, sizeof(event<>) * _fdcap);
    for (int i = _fdcap; i < ncap; i++)
	(void) new(static_cast<void *>(&nfd[i])) event<>();
    
    delete[] reinterpret_cast<unsigned char *>(_fd);
    _fd = nfd;
    _fdcap = ncap;
}

void driver::expand_asap()
{
    int ncap = (_asapcap ? _asapcap * 2 : 16);
    event<> *nasap = reinterpret_cast<event<> *>(new unsigned char[sizeof(event<>) * ncap]);
    memcpy(nasap, _asap, sizeof(event<>) * _asapcap);
    
    delete[] reinterpret_cast<unsigned char *>(_asap);
    _asap = nasap;
    _asapcap = ncap;
}

void driver::at_fd(int fd, bool write, const event<> &trigger)
{
    if (fd*2 >= _fdcap)
	expand_fds(fd);
    _fd[fd*2+write] = trigger;
    fd_set *fset = (write ? &_writefds : &_readfds);
    if (trigger) {
	//trigger.setcancel(event<>(_fdcancel, fd*2+write));
	FD_SET(fd, fset);
	if (fd >= _nfds)
	    _nfds = fd + 1;
    } else
	FD_CLR(fd, fset);
}

extern "C" {
static void tame_signal_handler(int signal) {
    sig_any_active = sig_active[signal] = 1;
    // ensure select wakes up, even if we get a signal in between setting the
    // timeout and calling select!
    write(sig_pipe[1], "", 1);

    // block signal until we trigger the event, giving the unblocked
    // rendezvous a chance to maybe install another handler
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, signal);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
}
}

void driver::at_signal(int signal, const event<> &trigger)
{
    assert(signal < NSIGNALS);
    
    if (trigger && sig_pipe[0] < 0)
	pipe(sig_pipe);

    sig_handlers[signal] = trigger;
    
    struct sigaction sa;
    sa.sa_handler = (trigger ? tame_signal_handler : SIG_DFL);
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    sigaction(signal, &sa, 0);
}

void driver::once()
{
    // get rid of initial cancelled timers
    ttimer *t;
    while (_nt > 0 && (t = _t[0], !t->trigger)) {
	timer_reheapify_from(0, _t[_nt - 1], true);
	_nt--;
	t->~ttimer();
	t->u.next = _tfree;
	_tfree = t;
    }

    // determine timeout
    struct timeval to, *toptr;
    if (_nasap > 0
	|| (_nt > 0 && !timercmp(&_t[0]->expiry, &now, >))
	|| sig_any_active) {
	timerclear(&to);
	toptr = &to;
    } else if (_nt == 0)
	toptr = 0;
    else {
#if 1
	for (int i = 0; i < _nt; i++)
	    fprintf(stderr, "%d.%06d ; ", _t[i]->expiry.tv_sec, _t[i]->expiry.tv_usec);
	fprintf(stderr, "(%d.%06d) \n", now.tv_sec, now.tv_usec);
#endif
	timersub(&_t[0]->expiry, &now, &to);
	toptr = &to;
    }

    // get rid of dead descriptors
    while (_nfds && !_fd[_nfds*2 - 2] && !_fd[_nfds*2 - 1])
	_nfds--;
    
    // select!
    fd_set rfds = _readfds;
    fd_set wfds = _writefds;
    int nfds = _nfds;
    if (sig_pipe[0] >= 0) {
	FD_SET(sig_pipe[0], &rfds);
	if (sig_pipe[0] > nfds)
	    nfds = sig_pipe[0] + 1;
    }
    nfds = select(nfds, &rfds, &wfds, 0, toptr);

    // run signals
    if (sig_any_active) {
	sig_any_active = 0;
	sigset_t sigs_unblock;
	sigemptyset(&sigs_unblock);

	// check signals
	for (int sig = 0; sig < NSIGNALS; sig++)
	    if (sig_active[sig]) {
		sig_active[sig] = 0;
		sig_handlers[sig].trigger();
		sigaddset(&sigs_unblock, sig);
	    }

	// run closures activated by signals (plus maybe some others)
	while (_rendezvous_base *r = _rendezvous_base::unblocked)
	    r->_run();

	// now that the signal responders have potentially reinstalled signal
	// handlers, unblock the signals
	sigprocmask(SIG_UNBLOCK, &sigs_unblock, 0);

	// kill crap data written to pipe
	char crap[64];
	while (read(sig_pipe[0], crap, 64) > 0)
	    /* do nothing */;
    }

    // run asaps
    while (_nasap > 0) {
	--_nasap;
	_asap[_nasap].trigger();
	_asap[_nasap].~event();
    }

    // run the file descriptors
    if (nfds >= 0) {
	for (int fd = 0; fd < _nfds; fd++) {
	    if (FD_ISSET(fd, &rfds)) {
		FD_CLR(fd, &_readfds);
		_fd[fd*2].trigger();
	    }
	    if (FD_ISSET(fd, &wfds)) {
		FD_CLR(fd, &_writefds);
		_fd[fd*2+1].trigger();
	    }
	}
    }

    // run the timers that worked
    gettimeofday(&now, 0);
    while (_nt > 0 && (t = _t[0], !timercmp(&t->expiry, &now, >))) {
	timer_reheapify_from(0, _t[_nt - 1], true);
	_nt--;
	t->trigger.trigger();
	t->~ttimer();
	t->u.next = _tfree;
	_tfree = t;
    }

    // run active closures
    while (_rendezvous_base *r = _rendezvous_base::unblocked)
	r->_run();
}

}