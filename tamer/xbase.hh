#ifndef TAMER_XBASE_HH
#define TAMER_XBASE_HH 1
/* Copyright (c) 2007-2012, Eddie Kohler
 * Copyright (c) 2007, Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Tamer LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Tamer LICENSE file; the license in that file is
 * legally binding.
 */
#include <stdexcept>
#include <stdint.h>
#include <assert.h>
#include <tamer/autoconf.h>
namespace tamer {

// Improve error messages from overloaded functions.
template <typename R> class two_argument_rendezvous_tag {};
template <typename R> class one_argument_rendezvous_tag {};
template <typename R> class zero_argument_rendezvous_tag {};

template <typename I0=void, typename I1=void> class rendezvous;
template <typename T0=void, typename T1=void, typename T2=void, typename T3=void> class event;
class driver;
class explicit_rendezvous;
class gather_rendezvous;

class tamer_error : public std::runtime_error { public:
    explicit tamer_error(const std::string &arg)
	: runtime_error(arg) {
    }
};

struct no_slot {
};

enum rendezvous_flags {
    rnormal,
    rvolatile
};

namespace tamerpriv {

class simple_event;
class abstract_rendezvous;
class explicit_rendezvous;
struct tamer_closure;
struct tamer_debug_closure;

class simple_event { public:

    // DO NOT derive from this class!

    typedef bool (simple_event::*unspecified_bool_type)() const;

    inline simple_event() TAMER_NOEXCEPT;
    template <typename R, typename I0, typename I1>
    inline simple_event(R &r, const I0 &i0, const I1 &i1) TAMER_NOEXCEPT;
    template <typename R, typename I0>
    inline simple_event(R &r, const I0 &i0) TAMER_NOEXCEPT;
    template <typename R>
    inline simple_event(R &r) TAMER_NOEXCEPT;
#if TAMER_DEBUG
    inline ~simple_event() TAMER_NOEXCEPT;
#endif

    inline void initialize(abstract_rendezvous *r, uintptr_t rid);
    inline void annotate(const char *file, int line);

    static inline void use(simple_event *e) TAMER_NOEXCEPT;
    static inline void unuse(simple_event *e) TAMER_NOEXCEPT;
    static inline void unuse_clean(simple_event *e) TAMER_NOEXCEPT;

    inline operator unspecified_bool_type() const;
    inline bool empty() const;
    inline abstract_rendezvous *rendezvous() const;
    inline uintptr_t rid() const;
    inline simple_event *next() const;

    inline void simple_trigger(bool values);
    static void simple_trigger(simple_event *x, bool values) TAMER_NOEXCEPT;
    void trigger_list_for_remove() TAMER_NOEXCEPT;

    static inline void at_trigger(simple_event *x, simple_event *at_trigger);
    static inline void at_trigger(simple_event *x, void (*f)(void *, int),
				  void *arg1, int arg2);

  protected:

    abstract_rendezvous *_r;
    uintptr_t _rid;
    simple_event *_r_next;
    simple_event **_r_pprev;
    void *at_trigger_;
    void (*at_trigger_f_)(void *, int);
    int at_trigger_arg2_;
    unsigned _refcount;
#if TAMER_DEBUG
    const char *annotate_file_;
    int annotate_line_;
#endif

    simple_event(const simple_event &);
    simple_event &operator=(const simple_event &);

    void trigger_for_unuse() TAMER_NOEXCEPT;
    simple_event *at_trigger_event();
    static void hard_at_trigger(simple_event *x, simple_event *at_trigger);
    static void hard_at_trigger(simple_event *x, void (*f)(void *, int),
				void *arg1, int arg2);

    friend class explicit_rendezvous;

};


enum rendezvous_type {
    rgather,
    rexplicit,
    rfunctional,
    rdistribute
};

class abstract_rendezvous {
  public:
    abstract_rendezvous(rendezvous_flags flags, rendezvous_type rtype)
	: waiting_(0), _blocked_closure(0),
	  rtype_(rtype), is_volatile_(flags == rvolatile) {
    }
    inline ~abstract_rendezvous() TAMER_NOEXCEPT;

    inline rendezvous_type rtype() const {
	return rendezvous_type(rtype_);
    }

    inline bool is_volatile() const {
	return is_volatile_;
    }
    inline void set_volatile(bool v) {
	is_volatile_ = v;
    }

    tamer_closure *linked_closure() const;
    inline tamer_closure *blocked_closure() const {
	return _blocked_closure;
    }

    inline void block(tamer_closure &c, unsigned position,
		      const char *file, int line);
    inline void block(tamer_debug_closure &c, unsigned position,
		      const char *file, int line);
    inline void unblock();
    inline void run();

    static inline bool has_unblocked() {
	return unblocked;
    }
    static inline abstract_rendezvous *pop_unblocked() {
	abstract_rendezvous *r = unblocked;
	if (r) {
	    if (!(unblocked = r->unblocked_next_))
		unblocked_ptail = &unblocked;
	}
	return r;
    }

  protected:
    simple_event *waiting_;
    tamer_closure *_blocked_closure;
    uint8_t rtype_;
    bool is_volatile_;
    abstract_rendezvous *unblocked_next_;

    static abstract_rendezvous *unblocked;
    static abstract_rendezvous **unblocked_ptail;
    static inline abstract_rendezvous *unblocked_sentinel() {
	return reinterpret_cast<abstract_rendezvous *>(uintptr_t(1));
    }

    inline void remove_waiting() TAMER_NOEXCEPT;

  private:
    abstract_rendezvous(const abstract_rendezvous &);
    abstract_rendezvous &operator=(const abstract_rendezvous &);
    void hard_free();

    friend class simple_event;
    friend class driver;
};

class explicit_rendezvous : public abstract_rendezvous {
  public:
    inline explicit_rendezvous(rendezvous_flags flags)
	: abstract_rendezvous(flags, rexplicit),
	  ready_(), ready_ptail_(&ready_) {
    }
#if TAMER_DEBUG
    inline ~explicit_rendezvous() {
	assert(!ready_);
    }
#endif

  protected:
    simple_event *ready_;
    simple_event **ready_ptail_;

    inline uintptr_t pop_ready() {
	simple_event *e = ready_;
	if (!(ready_ = e->_r_next))
	    ready_ptail_ = &ready_;
	uintptr_t x = e->rid();
	simple_event::unuse_clean(e);
	return x;
    }
    inline void remove_ready() {
	while (simple_event *e = ready_) {
	    ready_ = e->_r_next;
	    simple_event::unuse_clean(e);
	}
	ready_ptail_ = &ready_;
    }

    friend class simple_event;
};

class functional_rendezvous : public abstract_rendezvous {
  public:
    typedef void (*hook_type)(functional_rendezvous *fr,
			      simple_event *e, bool values);

    inline functional_rendezvous(void (*f)(functional_rendezvous *fr,
					   simple_event *e, bool values) TAMER_NOEXCEPT)
	: abstract_rendezvous(rnormal, rfunctional), f_(f) {
    }
    inline functional_rendezvous(rendezvous_type rtype,
				 void (*f)(functional_rendezvous *fr,
					   simple_event *e, bool values) TAMER_NOEXCEPT)
	: abstract_rendezvous(rnormal, rtype), f_(f) {
    }
    inline ~functional_rendezvous() {
	remove_waiting();
    }

  private:
    void (*f_)(functional_rendezvous *r,
	       simple_event *e, bool values) TAMER_NOEXCEPT;

    friend class simple_event;
};

typedef void (*tamer_closure_activator)(tamer_closure *);

struct tamer_closure {
    tamer_closure(tamer_closure_activator activator)
	: tamer_activator_(activator), tamer_block_position_(0) {
    }
    tamer_closure_activator tamer_activator_;
    unsigned tamer_block_position_;
};

template <typename T>
class closure_owner {
  public:
    inline closure_owner(T &c)
	: c_(&c) {
    }
    inline ~closure_owner() {
	delete c_;
    }
    inline void reset() {
	c_ = 0;
    }
  private:
    T *c_;
};

template <typename R>
class rendezvous_owner {
  public:
    inline rendezvous_owner(R &r)
	: r_(&r) {
    }
    inline ~rendezvous_owner() {
	if (r_)
	    r_->clear();
    }
    inline void reset() {
	r_ = 0;
    }
  private:
    R *r_;
};


struct tamer_debug_closure : public tamer_closure {
    tamer_debug_closure(tamer_closure_activator activator)
	: tamer_closure(activator) {
    }
    const char *tamer_blocked_file_;
    int tamer_blocked_line_;
};


namespace message {
void event_prematurely_dereferenced(simple_event *e, abstract_rendezvous *r);
}


inline void abstract_rendezvous::remove_waiting() TAMER_NOEXCEPT {
    if (waiting_) {
	waiting_->trigger_list_for_remove();
	waiting_ = 0;
    }
}

inline abstract_rendezvous::~abstract_rendezvous() TAMER_NOEXCEPT {
    // take all events off this rendezvous and call their triggerers
#if TAMER_DEBUG
    assert(!waiting_);
#endif
    if (_blocked_closure)
	hard_free();
}

inline void abstract_rendezvous::block(tamer_closure &c,
				       unsigned position,
				       const char *, int) {
    assert(!_blocked_closure && &c);
    _blocked_closure = &c;
    unblocked_next_ = unblocked_sentinel();
    c.tamer_block_position_ = position;
}

inline void abstract_rendezvous::block(tamer_debug_closure &c,
				       unsigned position,
				       const char *file, int line) {
    block(static_cast<tamer_closure &>(c), -position, file, line);
    c.tamer_blocked_file_ = file;
    c.tamer_blocked_line_ = line;
}

inline void abstract_rendezvous::unblock() {
    if (_blocked_closure && unblocked_next_ == unblocked_sentinel()) {
	*unblocked_ptail = this;
	unblocked_next_ = 0;
	unblocked_ptail = &unblocked_next_;
    }
}

inline void abstract_rendezvous::run() {
    tamer_closure *c = _blocked_closure;
    _blocked_closure = 0;
    c->tamer_activator_(c);
}


inline simple_event::simple_event() TAMER_NOEXCEPT
    : _r(0), _refcount(1) {
#if TAMER_DEBUG
    annotate_file_ = 0;
    annotate_line_ = 0;
#endif
}

template <typename R, typename I0, typename I1>
inline simple_event::simple_event(R &r, const I0 &i0, const I1 &i1) TAMER_NOEXCEPT
    : _refcount(1) {
#if TAMER_DEBUG
    _r = 0;
    annotate_file_ = 0;
    annotate_line_ = 0;
#endif
    r.add(this, i0, i1);
}

template <typename R, typename I0>
inline simple_event::simple_event(R &r, const I0 &i0) TAMER_NOEXCEPT
    : _refcount(1) {
#if TAMER_DEBUG
    _r = 0;
    annotate_file_ = 0;
    annotate_line_ = 0;
#endif
    r.add(this, i0);
}

template <typename R>
inline simple_event::simple_event(R &r) TAMER_NOEXCEPT
    : _refcount(1) {
#if TAMER_DEBUG
    _r = 0;
    annotate_file_ = 0;
    annotate_line_ = 0;
#endif
    r.add(this);
}

#if TAMER_DEBUG
inline simple_event::~simple_event() TAMER_NOEXCEPT {
    assert(!_r);
# if TAMER_DEBUG > 1
    if (annotate_file_ && annotate_line_)
	fprintf(stderr, "destroy simple_event(%p) %s:%d\n", this, annotate_file_, annotate_line_);
    else if (annotate_file_)
	fprintf(stderr, "destroy simple_event(%p) %s\n", this, annotate_file_);
# endif
}
#endif

inline void simple_event::initialize(abstract_rendezvous *r, uintptr_t rid) {
#if TAMER_DEBUG
    assert(_r == 0 && r != 0);
#endif
    // NB this can be called before e has been fully initialized.
    _r = r;
    _rid = rid;
    _r_pprev = &r->waiting_;
    if (r->waiting_)
	r->waiting_->_r_pprev = &_r_next;
    _r_next = r->waiting_;
    at_trigger_ = 0;
    at_trigger_f_ = 0;
    r->waiting_ = this;
}

inline void simple_event::use(simple_event *e) TAMER_NOEXCEPT {
    if (e)
	++e->_refcount;
}

inline void simple_event::unuse(simple_event *e) TAMER_NOEXCEPT {
    if (e && --e->_refcount == 0) {
	if (e->_r)
	    e->trigger_for_unuse();
	delete e;
    }
}

inline void simple_event::unuse_clean(simple_event *e) TAMER_NOEXCEPT {
    if (e && --e->_refcount == 0)
	delete e;
}

inline simple_event::operator unspecified_bool_type() const {
    return _r ? &simple_event::empty : 0;
}

inline bool simple_event::empty() const {
    return _r == 0;
}

inline void simple_event::annotate(const char *file, int line) {
#if TAMER_DEBUG
    annotate_file_ = file;
    annotate_line_ = line;
# if TAMER_DEBUG > 1
    if (file && line)
	fprintf(stderr, "annotate simple_event(%p) %s:%d\n", this, file, line);
    else if (file)
	fprintf(stderr, "annotate simple_event(%p) %s\n", this, file);
# endif
#else
    (void) file, (void) line;
#endif
}

inline abstract_rendezvous *simple_event::rendezvous() const {
    return _r;
}

inline uintptr_t simple_event::rid() const {
    return _rid;
}

inline simple_event *simple_event::next() const {
    return _r_next;
}

inline void simple_event::simple_trigger(bool values) {
    simple_trigger(this, values);
}

inline void simple_event::at_trigger(simple_event *x, simple_event *at_e) {
    if (x && *x && !x->at_trigger_ && at_e)
	x->at_trigger_ = at_e;
    else
	hard_at_trigger(x, at_e);
}

inline void simple_event::at_trigger(simple_event *x, void (*f)(void *, int),
				     void *arg1, int arg2) {
#if TAMER_DEBUG
    assert(arg1);
#endif
    if (x && *x && !x->at_trigger_) {
	x->at_trigger_ = arg1;
	x->at_trigger_f_ = f;
	x->at_trigger_arg2_ = arg2;
    } else
	hard_at_trigger(x, f, arg1, arg2);
}

template <typename T> struct rid_cast {
    static inline uintptr_t in(T x) TAMER_NOEXCEPT {
	return static_cast<uintptr_t>(x);
    }
    static inline T out(uintptr_t x) TAMER_NOEXCEPT {
	return static_cast<T>(x);
    }
};

template <typename T> struct rid_cast<T *> {
    static inline uintptr_t in(T *x) TAMER_NOEXCEPT {
	return reinterpret_cast<uintptr_t>(x);
    }
    static inline T *out(uintptr_t x) TAMER_NOEXCEPT {
	return reinterpret_cast<T *>(x);
    }
};

} // namespace tamerpriv
} // namespace tamer
#endif /* TAMER_BASE_HH */
