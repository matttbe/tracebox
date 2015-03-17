#ifndef __LUA_BASE_HPP_
#define __LUA_BASE_HPP_


#include <crafter.h>

#define LUA_COMPAT_ALL
#include <lua.hpp>

extern void stackDump (lua_State *L, const char* file, size_t line, std::ostream& out);
#define L_DUMP_STACK(l, s) stackDump(l, __FILE__, __LINE__, std::cerr)

extern void l_do(lua_State *l, const char*);

extern const char *l_classname_field;

/* Wrapper around lua types */
template<typename T>
struct l_data_type {
		T val;

		l_data_type() : val() {}
		virtual ~l_data_type() {}
		l_data_type(const T& v) : val(v) {}
		l_data_type& operator=(const T& v) { val = v; return *this; }
		l_data_type& operator=(const l_data_type<T>& v) { val = v; return *this; }
		operator T() const { return val; }
		void push(lua_State *l);
		static T get(lua_State *l, int n);
};

/* Assumes a (meta)table is on top of the stack */
template<class C>
void metatable_bind(lua_State *l, const char *key, l_data_type<C> data)
{
	data.push(l);
	lua_setfield(l, -2, key);
}
extern void meta_bind_func(lua_State *l, const char *key, lua_CFunction f);

template<class C>
struct tname {
	static const char *name;
};
#define TNAME(C) tname<C>::name
#define L_EXPOSE_TYPE(x) template<> const char *tname<x>::name = #x

struct _ref_count {
	size_t c;

	_ref_count() : c(0) {}
	void inc() { ++c; }
	size_t dec() { return --c; }
};

struct _ref_base {
	_ref_count *ref;
	void (_ref_base::*_del)();

	_ref_base() : ref(new _ref_count), _del(NULL) { retain(); }
	_ref_base(void (_ref_base::*f)()) : ref(new _ref_count), _del(f) { retain(); }
	_ref_base(const _ref_base& r) : ref(r.ref), _del(r._del) { retain(); }
	void retain() { ref->inc(); }

	virtual ~_ref_base() { release(); }
	void release() { if (!ref->dec()) { if (_del) (this->*_del)(); delete ref; } }

	virtual void debug(std::ostream&) = 0;
};

/* Base wrapper class to expose cpp object pointers to lua */
template<class C>
struct l_ref : public _ref_base {
	C *val;
	lua_State *l;
	/* Original owner of the pointer, if its not ourselves */
	_ref_base *aux_ref;

	/* Empty reference */
	l_ref() : val(NULL), l(NULL), aux_ref(NULL) {}
	/* New reference */
	l_ref(C *instance, lua_State *l)
		: val(instance), l(l), aux_ref(NULL) { push(l, this); }
	/* Copy reference */
	l_ref(l_ref *r) : _ref_base(*r), val(r->val), l(r->l), aux_ref(NULL){
		if (r->aux_ref) {
			aux_ref = r->aux_ref;
			aux_ref->retain();
		}
		push(l, this);
	}
	/* New reference, and register dependance to other one */
	template<class T>
	l_ref(l_ref<T> *r, C *i) : val(i), l(r->l), aux_ref(r)
	{
		aux_ref->retain();
		push(l, this);
	}

	operator C*() { return val; }

	static C* new_ref(lua_State *l)
	{
		C *o = new C();
		new l_ref(o, l);
		return o;
	}
	void _ref_expired()
	{
		if (!aux_ref)
			delete val;
	}

	~l_ref()
	{
		if (aux_ref) {
			if (aux_ref->ref->c == 1)
				delete aux_ref;
			else
				aux_ref->release();
		}
	}

	virtual void debug(std::ostream &out)
	{
		out << "[" << TNAME(C) << "] ";
	}

	C& operator* () { return *this->val; }
    C* operator-> () { return this->val; }

	l_ref& operator=(const l_ref& v)
	{
		if (this != &v) {
			this->release();
			this->val = v.val;
			ref = v.ref;
			ref->inc();
			aux_ref = v.aux_ref;
			if (aux_ref)
				aux_ref->retain();
		}
		return *this;
	}
	l_ref& operator=(const l_ref* v) { return this->operator=(*v); }


	static void push(lua_State *l, l_ref *r)
	{
		l_ref **udata = static_cast<l_ref **>(lua_newuserdata(l, sizeof(l_ref *)));
		*udata = r;
		luaL_getmetatable(l, TNAME(C));
		lua_setmetatable(l, -2);
	}
	void push(lua_State *l) { this->l = l; new l_ref(this); }

	static C* get(lua_State *l, int n) { return (C*)get_instance(l, n)->val; }
	static l_ref* get_instance(lua_State *l, int n)
	{
		return *static_cast<l_ref **>(luaL_checkudata(l, n, TNAME(C)));
	}

	static int destroy(lua_State *l)
	{
		delete l_ref::get_instance(l, 1);
		return 0;
	}

	/* Called to initialize this reference kind metatable */
	static void register_members(lua_State *l)
	{
		metatable_bind<const char*>(l, l_classname_field, l_data_type<const char*>(TNAME(C)));
		meta_bind_func(l, "__gc", destroy);
		/* Undocumented, for debug purposes */
		meta_bind_func(l, "__cpp_ref_count", _get_ref_count);
		meta_bind_func(l, "__cpp_auxref_count", _get_auxref_count);
	}

	/* Called once all types have registered */
	static void register_globals(lua_State *l) { (void)l; }

	static int _get_ref_count(lua_State *l)
	{
		l_ref *r = get_instance(l, 1);
		lua_pushnumber(l, r->ref->c);
		return 1;
	}

	static int _get_auxref_count(lua_State *l)
	{
		l_ref *r = get_instance(l, 1);
		if (r->aux_ref)
			lua_pushnumber(l, r->aux_ref->ref->c);
		else
			lua_pushnil(l);
		return 1;
	}
};

#endif
