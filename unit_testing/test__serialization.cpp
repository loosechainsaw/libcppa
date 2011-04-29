#include <new>
#include <locale>
#include <memory>
#include <string>
#include <limits>
#include <vector>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iterator>
#include <typeinfo>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <type_traits>

#include "test.hpp"

#include "cppa/util.hpp"
#include "cppa/match.hpp"
#include "cppa/tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/untyped_tuple.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"
#include "cppa/util/if_else_type.hpp"
#include "cppa/util/wrapped_type.hpp"

//#include "cppa/util.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace cppa::util;

template<class C, template <typename> class... Traits>
struct apply;

template<class C>
struct apply<C>
{
    typedef C type;
};

template<class C,
         template <typename> class Trait0,
         template <typename> class... Traits>
struct apply<C, Trait0, Traits...>
{
    typedef typename apply<typename Trait0<C>::type, Traits...>::type type;
};


template<typename T>
struct plain
{
    typedef typename apply<T, std::remove_reference, std::remove_cv>::type type;
};

/**
 * @brief Integers (signed and unsigned), floating points and strings.
 */
enum primitive_type
{
    pt_int8,        pt_int16,       pt_int32,        pt_int64,
    pt_uint8,       pt_uint16,      pt_uint32,       pt_uint64,
    pt_float,       pt_double,      pt_long_double,
    pt_u8string,    pt_u16string,   pt_u32string,
    pt_null
};

constexpr const char* primitive_type_names[] =
{
    "pt_int8",        "pt_int16",       "pt_int32",       "pt_int64",
    "pt_uint8",       "pt_uint16",      "pt_uint32",      "pt_uint64",
    "pt_float",       "pt_double",      "pt_long_double",
    "pt_u8string",    "pt_u16string",   "pt_u32string",
    "pt_null"
};

constexpr const char* primitive_type_name(primitive_type ptype)
{
    return primitive_type_names[static_cast<int>(ptype)];
}

// achieves static call dispatch (Int-To-Type idiom)
template<primitive_type FT>
struct pt_token { static const primitive_type value = FT; };

// maps the fundamental_type FT to the corresponding type
template<primitive_type FT>
struct ptype_to_type
  : if_else_type_c<FT == pt_int8, std::int8_t,
    if_else_type_c<FT == pt_int16, std::int16_t,
    if_else_type_c<FT == pt_int32, std::int32_t,
    if_else_type_c<FT == pt_int64, std::int64_t,
    if_else_type_c<FT == pt_uint8, std::uint8_t,
    if_else_type_c<FT == pt_uint16, std::uint16_t,
    if_else_type_c<FT == pt_uint32, std::uint32_t,
    if_else_type_c<FT == pt_uint64, std::uint64_t,
    if_else_type_c<FT == pt_float, float,
    if_else_type_c<FT == pt_double, double,
    if_else_type_c<FT == pt_long_double, long double,
    if_else_type_c<FT == pt_u8string, std::string,
    if_else_type_c<FT == pt_u16string, std::u16string,
    if_else_type_c<FT == pt_u32string, std::u32string,
    wrapped_type<void> > > > > > > > > > > > > > >
{
};

// if (IfStmt == true) ptype = FT; else ptype = Else::ptype;
template<bool IfStmt, primitive_type FT, class Else>
struct if_else_ptype_c
{
    static const primitive_type ptype = FT;
};

template<primitive_type FT, class Else>
struct if_else_ptype_c<false, FT, Else>
{
    static const primitive_type ptype = Else::ptype;
};

// if (Stmt::value == true) ptype = FT; else ptype = Else::ptype;
template<class Stmt, primitive_type FT, class Else>
struct if_else_ptype : if_else_ptype_c<Stmt::value, FT, Else> { };

template<primitive_type FT>
struct wrapped_ptype { static const primitive_type ptype = FT; };

// maps type T the the corresponding fundamental_type
template<typename T>
struct type_to_ptype_impl
      // signed integers
    : if_else_ptype<std::is_same<T, std::int8_t>, pt_int8,
      if_else_ptype<std::is_same<T, std::int16_t>, pt_int16,
      if_else_ptype<std::is_same<T, std::int32_t>, pt_int32,
      if_else_ptype<std::is_same<T, std::int64_t>, pt_int64,
      if_else_ptype<std::is_same<T, std::uint8_t>, pt_uint8,
      // unsigned integers
      if_else_ptype<std::is_same<T, std::uint16_t>, pt_uint16,
      if_else_ptype<std::is_same<T, std::uint32_t>, pt_uint32,
      if_else_ptype<std::is_same<T, std::uint64_t>, pt_uint64,
      // float / double
      if_else_ptype<std::is_same<T, float>, pt_float,
      if_else_ptype<std::is_same<T, double>, pt_double,
      if_else_ptype<std::is_same<T, long double>, pt_long_double,
      // strings
      if_else_ptype<std::is_convertible<T, std::string>, pt_u8string,
      if_else_ptype<std::is_convertible<T, std::u16string>, pt_u16string,
      if_else_ptype<std::is_convertible<T, std::u32string>, pt_u32string,
      wrapped_ptype<pt_null> > > > > > > > > > > > > > >
{
};

template<typename T>
struct type_to_ptype : type_to_ptype_impl<typename plain<T>::type> { };

namespace trait {

template<typename T>
struct is_primitive
{
    static const bool value = type_to_ptype<T>::ptype != pt_null;
};

template<typename T>
class is_iterable
{

    template<class C>
    static bool cmp_help_fun(C& arg0,
                             decltype((arg0.begin() == arg0.end()) &&
                                      (*(++(arg0.begin())) == *(arg0.end())))*)
    {
        return true;
    }

    template<class C>
    static void cmp_help_fun(C&, void*) { }

    typedef decltype(cmp_help_fun(*static_cast<T*>(nullptr),
                                  static_cast<bool*>(nullptr)))
            result_type;

 public:

    static const bool value =    !is_primitive<T>::value
                              && std::is_same<bool, result_type>::value;

};

} // namespace trait

class pt_value;

template<typename T>
T pt_value_cast(pt_value&);

template<primitive_type FT>
typename ptype_to_type<FT>::type& pt_value_cast(pt_value&);

// Utility function for static call dispatching.
// Invokes pt_token<X>(), where X is the value of ptype.
// Does nothing if ptype == pt_null
template<typename Fun>
void pt_invoke(primitive_type ptype, Fun&& f)
{
    switch (ptype)
    {
     case pt_int8:        f(pt_token<pt_int8>());        break;
     case pt_int16:       f(pt_token<pt_int16>());       break;
     case pt_int32:       f(pt_token<pt_int32>());       break;
     case pt_int64:       f(pt_token<pt_int64>());       break;
     case pt_uint8:       f(pt_token<pt_uint8>());       break;
     case pt_uint16:      f(pt_token<pt_uint16>());      break;
     case pt_uint32:      f(pt_token<pt_uint32>());      break;
     case pt_uint64:      f(pt_token<pt_uint64>());      break;
     case pt_float:       f(pt_token<pt_float>());       break;
     case pt_double:      f(pt_token<pt_double>());      break;
     case pt_long_double: f(pt_token<pt_long_double>()); break;
     case pt_u8string:    f(pt_token<pt_u8string>());    break;
     case pt_u16string:   f(pt_token<pt_u16string>());   break;
     case pt_u32string:   f(pt_token<pt_u32string>());   break;
     default: break;
    }
}

/**
 * @brief Describes a value of a {@link primitive_type primitive data type}.
 */
class pt_value
{

    template<typename T>
    friend T pt_value_cast(pt_value&);

    template<primitive_type PT>
    friend typename ptype_to_type<PT>::type& pt_value_cast(pt_value&);

    primitive_type m_ptype;

    union
    {
        std::int8_t i8;
        std::int16_t i16;
        std::int32_t i32;
        std::int64_t i64;
        std::uint8_t u8;
        std::uint16_t u16;
        std::uint32_t u32;
        std::uint64_t u64;
        float fl;
        double dbl;
        long double ldbl;
        std::string str8;
        std::u16string str16;
        std::u32string str32;
    };

    // use static call dispatching to select member variable
    inline decltype(i8)&    get(pt_token<pt_int8>)        { return i8;    }
    inline decltype(i16)&   get(pt_token<pt_int16>)       { return i16;   }
    inline decltype(i32)&   get(pt_token<pt_int32>)       { return i32;   }
    inline decltype(i64)&   get(pt_token<pt_int64>)       { return i64;   }
    inline decltype(u8)&    get(pt_token<pt_uint8>)       { return u8;    }
    inline decltype(u16)&   get(pt_token<pt_uint16>)      { return u16;   }
    inline decltype(u32)&   get(pt_token<pt_uint32>)      { return u32;   }
    inline decltype(u64)&   get(pt_token<pt_uint64>)      { return u64;   }
    inline decltype(fl)&    get(pt_token<pt_float>)       { return fl;    }
    inline decltype(dbl)&   get(pt_token<pt_double>)      { return dbl;   }
    inline decltype(ldbl)&  get(pt_token<pt_long_double>) { return ldbl;  }
    inline decltype(str8)&  get(pt_token<pt_u8string>)    { return str8;  }
    inline decltype(str16)& get(pt_token<pt_u16string>)   { return str16; }
    inline decltype(str32)& get(pt_token<pt_u32string>)   { return str32; }

    template<primitive_type FT, class T, class V>
    static void set(primitive_type& lhs_type, T& lhs, V&& rhs,
                    typename disable_if<std::is_arithmetic<T>>::type* = 0)
    {
        if (FT == lhs_type)
        {
            lhs = std::forward<V>(rhs);
        }
        else
        {
            new (&lhs) T(std::forward<V>(rhs));
            lhs_type = FT;
        }
    }

    template<primitive_type FT, class T, class V>
    static void set(primitive_type& lhs_type, T& lhs, V&& rhs,
                    typename enable_if<std::is_arithmetic<T>>::type* = 0)
    {
        // don't call a constructor for arithmetic types
        lhs = rhs;
        lhs_type = FT;
    }

    template<class T>
    static void destroy(T& what,
                        typename disable_if<std::is_arithmetic<T>>::type* = 0)
    {
        what.~T();
    }

    template<class T>
    static void destroy(T&,
                        typename enable_if<std::is_arithmetic<T>>::type* = 0)
    {
        // arithmetic types don't need destruction
    }

    struct destroyer
    {
        pt_value* m_self;
        inline destroyer(pt_value* self) : m_self(self) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token) const
        {
            destroy(m_self->get(token));
        }
    };

    struct initializer
    {
        pt_value* m_self;
        inline initializer(pt_value* self) : m_self(self) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token) const
        {
            set<FT>(m_self->m_ptype,
                    m_self->get(token),
                    typename ptype_to_type<FT>::type());
        }
    };

    struct setter
    {
        pt_value* m_self;
        const pt_value& m_other;
        inline setter(pt_value* self, const pt_value& other)
            : m_self(self), m_other(other) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token) const
        {
            set<FT>(m_self->m_ptype,
                    m_self->get(token),
                    m_other.get(token));
        }
    };

    struct mover
    {
        pt_value* m_self;
        const pt_value& m_other;
        inline mover(pt_value* self, const pt_value& other)
            : m_self(self), m_other(other) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token) const
        {
            set<FT>(m_self->m_ptype,
                    m_self->get(token),
                    std::move(m_other.get(token)));
        }
    };

    struct comparator
    {
        bool m_result;
        const pt_value* m_self;
        const pt_value& m_other;
        inline comparator(const pt_value* self, const pt_value& other)
            : m_result(false), m_self(self), m_other(other) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token)
        {
            if (m_other.m_ptype == FT)
            {
                m_result = (m_self->get(token) == m_other.get(token));
            }
        }
        inline bool result() const { return m_result; }
    };

    template<class Self, typename Fun>
    struct applier
    {
        Self* m_self;
        Fun& m_f;
        applier(Self* self, Fun& f) : m_self(self), m_f(f) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT> token)
        {
            m_f(m_self->get(token));
        }
    };

    struct type_reader
    {
        const std::type_info* tinfo;
        type_reader() : tinfo(nullptr) { }
        template<primitive_type FT>
        inline void operator()(pt_token<FT>)
        {
            tinfo = &typeid(typename ptype_to_type<FT>::type);
        }
    };

    void destroy()
    {
        pt_invoke(m_ptype, destroyer(this));
        m_ptype = pt_null;
    }

 public:

    // get(...) const overload
    template<primitive_type FT>
    const typename ptype_to_type<FT>::type& get(pt_token<FT> token) const
    {
        return const_cast<pt_value*>(this)->get(token);
    }

    template<typename Fun>
    void apply(Fun&& f)
    {
        pt_invoke(m_ptype, applier<pt_value, Fun>(this, f));
    }

    template<typename Fun>
    void apply(Fun&& f) const
    {
        pt_invoke(m_ptype, applier<const pt_value, Fun>(this, f));
    }

    pt_value() : m_ptype(pt_null) { }

    template<typename V>
    pt_value(V&& value) : m_ptype(pt_null)
    {
        static_assert(type_to_ptype<V>::ptype != pt_null,
                      "T couldn't be mapped to an ptype");
        set<type_to_ptype<V>::ptype>(m_ptype,
                                     get(pt_token<type_to_ptype<V>::ptype>()),
                                     std::forward<V>(value));
    }

    pt_value(primitive_type ptype) : m_ptype(pt_null)
    {
        pt_invoke(ptype, initializer(this));
    }

    pt_value(const pt_value& other) : m_ptype(pt_null)
    {
        //invoke(setter(other));
        pt_invoke(other.m_ptype, setter(this, other));
    }

    pt_value(pt_value&& other) : m_ptype(pt_null)
    {
        //invoke(mover(other));
        pt_invoke(other.m_ptype, mover(this, other));
    }

    pt_value& operator=(const pt_value& other)
    {
        //invoke(setter(other));
        pt_invoke(other.m_ptype, setter(this, other));
        return *this;
    }

    pt_value& operator=(pt_value&& other)
    {
        //invoke(mover(other));
        pt_invoke(other.m_ptype, mover(this, other));
        return *this;
    }

    bool operator==(const pt_value& other) const
    {
        comparator cmp(this, other);
        pt_invoke(m_ptype, cmp);
        return cmp.result();
    }

    inline bool operator!=(const pt_value& other) const
    {
        return !(*this == other);
    }

    inline primitive_type ptype() const { return m_ptype; }

    const std::type_info& type() const
    {
        type_reader tr;
        pt_invoke(m_ptype, tr);
        return (tr.tinfo) ? *tr.tinfo : typeid(void);
    }

    ~pt_value() { destroy(); }

};

template<typename T>
typename enable_if<trait::is_primitive<T>, bool>::type
operator==(const T& lhs, const pt_value& rhs)
{
    constexpr primitive_type ptype = type_to_ptype<T>::ptype;
    static_assert(ptype != pt_null, "T couldn't be mapped to an ptype");
    return (rhs.ptype() == ptype) ? lhs == pt_value_cast<ptype>(rhs) : false;
}

template<typename T>
typename enable_if<trait::is_primitive<T>, bool>::type
operator==(const pt_value& lhs, const T& rhs)
{
    return (rhs == lhs);
}

template<typename T>
typename enable_if<trait::is_primitive<T>, bool>::type
operator!=(const pt_value& lhs, const T& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
typename enable_if<trait::is_primitive<T>, bool>::type
operator!=(const T& lhs, const pt_value& rhs)
{
    return !(lhs == rhs);
}

template<primitive_type FT>
typename ptype_to_type<FT>::type& pt_value_cast(pt_value& v)
{
    if (v.ptype() != FT) throw std::bad_cast();
    return v.get(pt_token<FT>());
}

template<primitive_type FT>
const typename ptype_to_type<FT>::type& pt_value_cast(const pt_value& v)
{
    if (v.ptype() != FT) throw std::bad_cast();
    return v.get(pt_token<FT>());
}

template<typename T>
T pt_value_cast(pt_value& v)
{
    static const primitive_type ptype = type_to_ptype<T>::ptype;
    if (v.ptype() != ptype) throw std::bad_cast();
    return v.get(pt_token<ptype>());
}

template<typename T>
T pt_value_cast(const pt_value& v)
{
    typedef typename std::remove_reference<T>::type plain_t;
    static_assert(!std::is_reference<T>::value || std::is_const<plain_t>::value,
                  "Could not get a non-const reference from const pt_value&");
    static const primitive_type ptype = type_to_ptype<T>::ptype;
    if (v.ptype() != ptype) throw std::bad_cast();
    return v.get(pt_token<ptype>());
}

struct getter_setter_pair
{

    std::function<pt_value (void*)> getter;
    std::function<void (void*, pt_value&&)> setter;

    getter_setter_pair(getter_setter_pair&&) = default;
    getter_setter_pair(const getter_setter_pair&) = default;

    template<class C, typename T>
    getter_setter_pair(T C::*member_ptr)
    {
        getter = [=] (void* self) -> pt_value {
            return *reinterpret_cast<C*>(self).*member_ptr;
        };
        setter = [=] (void* self, pt_value&& value) {
            *reinterpret_cast<C*>(self).*member_ptr = std::move(pt_value_cast<T&>(value));
        };
    }

    template<class C, typename GT, typename ST>
    getter_setter_pair(GT (C::*get_memfn)() const, void (C::*set_memfn)(ST))
    {
        getter = [=] (void* self) -> pt_value {
            return (*reinterpret_cast<C*>(self).*get_memfn)();
        };
        setter = [=] (void* self, pt_value&& value) {
            (*reinterpret_cast<C*>(self).*set_memfn)(std::move(pt_value_cast<typename plain<ST>::type&>(value)));
        };
    }

};

class serializer
{

 public:

    virtual void begin_object(const std::string& type_name) = 0;
    virtual void end_object() = 0;

    virtual void begin_list(size_t size) = 0;
    virtual void end_list() = 0;

    virtual void write_value(const pt_value& value) = 0;

};

class serializer;
class deserializer;

class meta_type
{

 public:

    virtual ~meta_type() { }

    /**
     * @brief Create an instance of this type, initialized
     *        with its default constructor.
     */
    virtual void* default_constructed() = 0;
    virtual void delete_instance(void*) = 0;
    virtual void serialize(void*, serializer*) = 0;
    virtual void deserialize(void*, deserializer*) = 0;

};

std::map<std::string, meta_type*> s_meta_types;

/**
 *
 */
class deserializer
{

 public:

    /**
     * @brief Seek the beginning of the next object and return its type name.
     */
    virtual std::string seek_object() = 0;

    /**
     * @brief Seek the beginning of the next object and return its type name,
     *        but don't modify the internal in-stream position.
     */
    virtual std::string peek_object() = 0;

    virtual void begin_object(const std::string& type_name) = 0;
    virtual void end_object() = 0;

    virtual size_t begin_list(primitive_type value_type) = 0;
    virtual void end_list() = 0;

    virtual pt_value read_value(primitive_type ptype) = 0;

};

class root_object
{

 public:

    std::pair<void*, meta_type*> deserialize(deserializer* d)
    {
        void* result;
        std::string tname = d->peek_object();
        auto i = s_meta_types.find(tname);
        if (i == s_meta_types.end())
        {
            throw std::logic_error("no meta object found for " + tname);
        }
        auto mobj = i->second;
        if (mobj == nullptr)
        {
            throw std::logic_error("mobj == nullptr");
        }
        result = mobj->default_constructed();
        if (result == nullptr)
        {
            throw std::logic_error("result == nullptr");
        }
        try
        {
            mobj->deserialize(result, d);
        }
        catch (...)
        {
            mobj->delete_instance(result);
            return { nullptr, nullptr };
        }
        return { result, mobj };
    }

};

template<typename T>
class meta_value_property : public meta_type
{

    static constexpr primitive_type ptype = type_to_ptype<T>::ptype;

    static_assert(ptype != pt_null, "T is not a primitive type");

 public:

    meta_value_property() { }

    pt_value get(void* obj)
    {
        return *reinterpret_cast<T*>(obj);
    }

    void set(void* obj, pt_value&& value)
    {
        *reinterpret_cast<T*>(obj) = std::move(pt_value_cast<T&>(value));
    }

    void* default_constructed()
    {
        return new T();
    }

    void delete_instance(void* ptr)
    {
        delete reinterpret_cast<T*>(ptr);
    }

    void serialize(void* obj, serializer* s)
    {
        s->write_value(get(obj));
    }

    void deserialize(void* obj, deserializer* d)
    {
        set(obj, d->read_value(ptype));
    }

};

// std::vector or std::list
template<typename List>
class meta_list_property : public meta_type
{

    typedef typename List::value_type value_type;
    static constexpr primitive_type vptype = type_to_ptype<value_type>::ptype;

    static_assert(vptype != pt_null, "T doesn't have a primitive value_type");

 public:

    meta_list_property() { }

    void serialize(void* obj, serializer* s)
    {
        auto& ls = *reinterpret_cast<List*>(obj);
        s->begin_list(ls.size());
        for (const auto& val : ls)
        {
            s->write_value(val);
        }
        s->end_list();
    }

    void deserialize(void* obj, deserializer* d)
    {
        auto& ls = *reinterpret_cast<List*>(obj);
        size_t ls_size = d->begin_list(vptype);
        for (size_t i = 0; i < ls_size; ++i)
        {
            pt_value val = d->read_value(vptype);
            ls.push_back(std::move(pt_value_cast<value_type&>(val)));
        }
        d->end_list();
    }

    void* default_constructed()
    {
        return new List();
    }

    void delete_instance(void* ptr)
    {
        delete reinterpret_cast<List*>(ptr);
    }

};

template<class Object>
class meta_object : public meta_type
{

    class member
    {

        meta_type* m_meta;
        std::function<void* (void*)> m_deref;

     public:

        template<typename T, class C>
        member(meta_type* mtptr, T C::*mem_ptr) : m_meta(mtptr)
        {
            m_deref = [mem_ptr] (void* obj) -> void*
            {
                return &(*reinterpret_cast<C*>(obj).*mem_ptr);
            };
        }

        member(meta_type* pptr, std::function<void* (void*)>&& gpm)
            : m_meta(pptr), m_deref(std::move(gpm))
        {
        }

        member(member&&) = default;

        member(const member&) = default;

        inline void serialize(void* parent, serializer* s)
        {
            m_meta->serialize(m_deref(parent), s);
        }

        inline void deserialize(void* parent, deserializer* s)
        {
            m_meta->deserialize(m_deref(parent), s);
        }

    };

    std::string class_name;
    std::vector<member> m_members;

    // terminates recursion
    inline void push_back() { }

    template<typename T, class C, typename... Args>
    void push_back(std::pair<T C::*, meta_object<T>*> pr, const Args&... args)
    {
        m_members.push_back({ pr.second, pr.first });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename enable_if<trait::is_primitive<T> >::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        m_members.push_back({ new meta_value_property<T>(), mem_ptr });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename enable_if<trait::is_iterable<T> >::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        m_members.push_back({ new meta_list_property<T>(), mem_ptr });
        push_back(args...);
    }

    template<class C, typename T, typename... Args>
    typename disable_if_c<   trait::is_primitive<T>::value
                          || trait::is_iterable<T>::value, void>::type
    push_back(T C::*mem_ptr, const Args&... args)
    {
        static_assert(trait::is_primitive<T>::value,
                      "T is neither a primitive type nor an iterable type");
    }

 public:

    template<typename... Args>
    meta_object(const Args&... args) : class_name(cppa::detail::to_uniform_name(cppa::detail::demangle(typeid(Object).name())))
    {
        push_back(args...);
    }

    void serialize(void* obj, serializer* s)
    {
        s->begin_object(class_name);
        for (auto& m : m_members)
        {
            m.serialize(obj, s);
        }
        s->end_object();
    }

    void deserialize(void* obj, deserializer* d)
    {
        std::string cname = d->seek_object();
        if (cname != class_name)
        {
            throw std::logic_error("wrong type name found");
        }
        d->begin_object(class_name);
        for (auto& m : m_members)
        {
            m.deserialize(obj, d);
        }
        d->end_object();
    }

    void* default_constructed()
    {
        return new Object();
    }

    void delete_instance(void* ptr)
    {
        delete reinterpret_cast<Object*>(ptr);
    }

};

struct struct_a
{
    int x;
    int y;
};

bool operator==(const struct_a& lhs, const struct_a& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool operator!=(const struct_a& lhs, const struct_a& rhs)
{
    return !(lhs == rhs);
}

struct struct_b
{
    struct_a a;
    int z;
    std::list<int> ints;
};

bool operator==(const struct_b& lhs, const struct_b& rhs)
{
    return lhs.a == rhs.a && lhs.z == rhs.z && lhs.ints == rhs.ints;
}

bool operator!=(const struct_b& lhs, const struct_b& rhs)
{
    return !(lhs == rhs);
}

template<class C, class Parent, typename... Args>
std::pair<C Parent::*, meta_object<C>*> compound_member(C Parent::*c_ptr, const Args&... args)
{
    return std::make_pair(c_ptr, new meta_object<C>(args...));
}

class string_serializer : public serializer
{

    std::ostream& out;

    struct pt_writer
    {

        std::ostream& out;

        pt_writer(std::ostream& mout) : out(mout) { }

        template<typename T>
        void operator()(const T& value)
        {
            out << value;
        }

        void operator()(const std::string& str)
        {
            out << "\"" << str << "\"";
        }

        void operator()(const std::u16string&) { }

        void operator()(const std::u32string&) { }

    };

    int m_open_objects;
    bool m_after_value;

    inline void clear()
    {
        if (m_after_value)
        {
            out << ", ";
            m_after_value = false;
        }
    }

 public:

    string_serializer(std::ostream& mout)
        : out(mout), m_open_objects(0), m_after_value(false) { }

    void begin_object(const std::string& type_name)
    {
        clear();
        ++m_open_objects;
        out << type_name << " ( ";
    }
    void end_object()
    {
        out << " )";
    }

    void begin_list(size_t)
    {
        clear();
        out << "{ ";
    }

    void end_list()
    {
        if (!m_after_value)
        {
            out << "}";
        }
        else
        {
            out << " }";
        }
    }

    void write_value(const pt_value& value)
    {
        clear();
        value.apply(pt_writer(out));
        m_after_value = true;
    }

};

class xml_serializer : public serializer
{

    std::ostream& out;
    std::string indentation;

    inline void inc_indentation()
    {
        indentation.resize(indentation.size() + 4, ' ');
    }

    inline void dec_indentation()
    {
        auto isize = indentation.size();
        indentation.resize((isize > 4) ? (isize - 4) : 0);
    }

    struct pt_writer
    {

        std::ostream& out;
        const std::string& indentation;

        pt_writer(std::ostream& ostr, const std::string& indent)
            : out(ostr), indentation(indent)
        {
        }

        template<typename T>
        void operator()(const T& value)
        {
            out << indentation << "<value type=\""
                << primitive_type_name(type_to_ptype<T>::ptype)
                << "\">" << value << "</value>\n";
        }

        void operator()(const std::u16string&) { }

        void operator()(const std::u32string&) { }

    };

 public:

    xml_serializer(std::ostream& ostr) : out(ostr), indentation("") { }

    void begin_object(const std::string& type_name)
    {
        out << indentation << "<object type=\"" << type_name << "\">\n";
        inc_indentation();
    }
    void end_object()
    {
        dec_indentation();
        out << indentation << "</object>\n";
    }

    void begin_list(size_t)
    {
        out << indentation << "<list>\n";
        inc_indentation();
    }

    void end_list()
    {
        dec_indentation();
        out << indentation << "</list>\n";
    }

    void write_value(const pt_value& value)
    {
        value.apply(pt_writer(out, indentation));
    }

};

class binary_serializer : public serializer
{

    typedef char* buf_type;

    buf_type m_buf;
    size_t m_wr_pos;

    struct pt_writer
    {

        buf_type& m_buf;
        size_t& m_wr_pos;

        pt_writer(buf_type& buf, size_t& pos) : m_buf(buf), m_wr_pos(pos) { }

        template<typename T>
        void operator()(const T& value)
        {
            memcpy(m_buf + m_wr_pos, &value, sizeof(T));
            m_wr_pos += sizeof(T);
        }

        void operator()(const std::u16string&) { }

        void operator()(const std::u32string&) { }

    };

    template<typename T>
    void write(const T& value)
    {
        memcpy(m_buf + m_wr_pos, &value, sizeof(T));
        m_wr_pos += sizeof(T);
    }

    void write(const std::string& str)
    {
        write(static_cast<std::uint32_t>(str.size()));
        memcpy(m_buf + m_wr_pos, str.c_str(), str.size());
        m_wr_pos += str.size();
    }

 public:

    binary_serializer(char* buf) : m_buf(buf), m_wr_pos(0) { }

    void begin_object(const std::string& tname)
    {
        write(tname);
    }

    void end_object() { }

    void begin_list(size_t list_size)
    {
        write(static_cast<std::uint32_t>(list_size));
    }

    void end_list() { }

    void write_value(const pt_value& value)
    {
        value.apply(pt_writer(m_buf, m_wr_pos));
    }

};

class binary_deserializer : public deserializer
{

    typedef char* buf_type;

    buf_type m_buf;
    size_t m_rd_pos;
    size_t m_buf_size;

    void range_check(size_t read_size)
    {
        if (m_rd_pos + read_size >= m_buf_size)
        {
            std::out_of_range("binary_deserializer::read()");
        }
    }

    template<typename T>
    void read(T& storage, bool modify_rd_pos = true)
    {
        range_check(sizeof(T));
        memcpy(&storage, m_buf + m_rd_pos, sizeof(T));
        if (modify_rd_pos) m_rd_pos += sizeof(T);
    }

    void read(std::string& str, bool modify_rd_pos = true)
    {
        std::uint32_t str_size;
        read(str_size, modify_rd_pos);
        char* cpy_begin;
        if (modify_rd_pos)
        {
            range_check(str_size);
            cpy_begin = m_buf + m_rd_pos;
        }
        else
        {
            range_check(sizeof(std::uint32_t) + str_size);
            cpy_begin = m_buf + m_rd_pos + sizeof(std::uint32_t);
        }
        str.clear();
        str.reserve(str_size);
        char* cpy_end = cpy_begin + str_size;
        std::copy(cpy_begin, cpy_end, std::back_inserter(str));
        if (modify_rd_pos) m_rd_pos += str_size;
    }

    template<typename CharType, typename StringType>
    void read_unicode_string(StringType& str)
    {
        std::uint32_t str_size;
        read(str_size);
        str.reserve(str_size);
        for (size_t i = 0; i < str_size; ++i)
        {
            CharType c;
            read(c);
            str_size += static_cast<typename StringType::value_type>(c);
        }
    }

    void read(std::u16string& str)
    {
        // char16_t is guaranteed to has *at least* 16 bytes,
        // but not to have *exactly* 16 bytes; thus use uint16_t
        read_unicode_string<std::uint16_t>(str);
    }

    void read(std::u32string& str)
    {
        // char32_t is guaranteed to has *at least* 32 bytes,
        // but not to have *exactly* 32 bytes; thus use uint32_t
        read_unicode_string<std::uint32_t>(str);
    }

    struct pt_reader
    {
        binary_deserializer* self;
        inline pt_reader(binary_deserializer* mself) : self(mself) { }
        template<typename T>
        inline void operator()(T& value)
        {
            self->read(value);
        }
    };

 public:

    binary_deserializer(buf_type buf, size_t buf_size)
        : m_buf(buf), m_rd_pos(0), m_buf_size(buf_size)
    {
    }

    std::string seek_object()
    {
        std::string result;
        read(result);
        return result;
    }

    std::string peek_object()
    {
        std::string result;
        read(result, false);
        return result;
    }

    void begin_object(const std::string&)
    {
    }

    void end_object()
    {
    }

    size_t begin_list(primitive_type)
    {
        std::uint32_t size;
        read(size);
        return size;
    }

    void end_list()
    {
    }

    pt_value read_value(primitive_type ptype)
    {
        pt_value val(ptype);
        val.apply(pt_reader(this));
        return val;
    }

};

class string_deserializer : public deserializer
{

    std::string m_str;
    std::string::iterator m_pos;
    size_t m_obj_count;

    void skip_space_and_comma()
    {
        while (*m_pos == ' ' || *m_pos == ',') ++m_pos;
    }

    void throw_malformed(const std::string& error_msg)
    {
        throw std::logic_error("malformed string: " + error_msg);
    }

    void consume(char c)
    {
        skip_space_and_comma();
        if (*m_pos != c)
        {
            std::string error;
            error += "expected '";
            error += c;
            error += "' found '";
            error += *m_pos;
            error += "'";
            throw_malformed(error);
        }
        ++m_pos;
    }

    inline std::string::iterator next_delimiter()
    {
        return std::find_if(m_pos, m_str.end(), [] (char c) -> bool {
            switch (c)
            {
             case '(':
             case ')':
             case '{':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        });
    }

 public:

    string_deserializer(const std::string& str) : m_str(str)
    {
        m_pos = m_str.begin();
        m_obj_count = 0;
    }

    string_deserializer(std::string&& str) : m_str(std::move(str))
    {
        m_pos = m_str.begin();
        m_obj_count = 0;
    }

    std::string seek_object()
    {
        skip_space_and_comma();
        auto substr_end = next_delimiter();
        // next delimiter must eiter be '(' or "\w+\("
        if (substr_end == m_str.end() ||  *substr_end != '(')
        {
            auto peeker = substr_end;
            while (peeker != m_str.end() && *peeker == ' ') ++peeker;
            if (peeker == m_str.end() ||  *peeker != '(')
            {
                throw_malformed("type name not followed by '('");
            }
        }
        std::string result(m_pos, substr_end);
        m_pos = substr_end;
        return result;
    }

    std::string peek_object()
    {
        std::string result = seek_object();
        // restore position in stream
        m_pos -= result.size();
        return result;
    }

    void begin_object(const std::string&)
    {
        ++m_obj_count;
        skip_space_and_comma();
        consume('(');
    }

    void end_object()
    {
        consume(')');
        if (--m_obj_count == 0)
        {
            skip_space_and_comma();
            if (m_pos != m_str.end())
            {
                throw_malformed("expected end of of string");
            }
        }
    }

    size_t begin_list(primitive_type)
    {
        consume('{');
        auto list_end = std::find(m_pos, m_str.end(), '}');
        return std::count(m_pos, list_end, ',') + 1;
    }

    void end_list()
    {
        consume('}');
    }

    struct from_string
    {
        const std::string& str;
        from_string(const std::string& s) : str(s) { }
        template<typename T>
        void operator()(T& what)
        {
            std::istringstream s(str);
            s >> what;
        }
        void operator()(std::string& what)
        {
            what = str;
        }
        void operator()(std::u16string&) { }
        void operator()(std::u32string&) { }
    };

    pt_value read_value(primitive_type ptype)
    {
        skip_space_and_comma();
        auto substr_end = std::find_if(m_pos, m_str.end(), [] (char c) -> bool {
            switch (c)
            {
             case ')':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        });
        std::string substr(m_pos, substr_end);
        pt_value result(ptype);
        result.apply(from_string(substr));
        m_pos += substr.size();
        return result;
    }

};

template<typename T>
std::string to_string(T* what, meta_object<T>* mobj)
{
    std::ostringstream osstr;
    string_serializer strs(osstr);
    mobj->serialize(what, &strs);
    return osstr.str();
}

std::size_t test__serialization()
{

    CPPA_TEST(test__serialization);

    CPPA_CHECK_EQUAL((trait::is_iterable<int>::value), false);
    // std::string is primitive and thus not identified by is_iterable
    CPPA_CHECK_EQUAL((trait::is_iterable<std::string>::value), false);
    CPPA_CHECK_EQUAL((trait::is_iterable<std::list<int>>::value), true);
    CPPA_CHECK_EQUAL((trait::is_iterable<std::map<int,int>>::value), true);

    // test pt_value implementation
    {
        pt_value v1(42);
        pt_value v2(42);
        CPPA_CHECK_EQUAL(v1, v2);
        CPPA_CHECK_EQUAL(v1, 42);
        CPPA_CHECK_EQUAL(42, v2);
        // type mismatch => unequal
        CPPA_CHECK(v2 != static_cast<std::int8_t>(42));
    }

    root_object root;

    // test serializers / deserializers
    {

        meta_object<struct_b> meta_b {
            compound_member(&struct_b::a, &struct_a::x, &struct_a::y),
            &struct_b::z,
            &struct_b::ints
        };

        // "announce" meta types
        s_meta_types["struct_b"] = &meta_b;

        struct_b b1 = { { 1, 2 }, 3, { 4, 5, 6, 7, 8, 9, 10 } };
        struct_b b2;
        struct_b b3;

        auto b1str = "struct_b ( struct_a ( 1, 2 ), 3, "
                     "{ 4, 5, 6, 7, 8, 9, 10 } )";

        CPPA_CHECK_EQUAL((to_string(&b1, &meta_b)), b1str);

        char buf[512];

        // serialize b1 to buf
        {
            binary_serializer bs(buf);
            meta_b.serialize(&b1, &bs);
        }

        // deserialize b2 from buf
        {
            binary_deserializer bd(buf, 512);
            auto res = root.deserialize(&bd);
            CPPA_CHECK_EQUAL(res.second, &meta_b);
            if (res.second == &meta_b)
            {
                b2 = *reinterpret_cast<struct_b*>(res.first);
            }
            res.second->delete_instance(res.first);
        }

        CPPA_CHECK_EQUAL(b1, b2);
        CPPA_CHECK_EQUAL((to_string(&b2, &meta_b)), b1str);

        // deserialize b3 from string, using string_deserializer
        {
            string_deserializer strd(b1str);
            auto res = root.deserialize(&strd);
            CPPA_CHECK_EQUAL(res.second, &meta_b);
            if (res.second == &meta_b)
            {
                b3 = *reinterpret_cast<struct_b*>(res.first);
            }
            res.second->delete_instance(res.first);
        }

        CPPA_CHECK_EQUAL(b1, b3);

    }

    return CPPA_TEST_RESULT;

}