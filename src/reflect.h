/* reflect.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 08 Mar 2014
   FreeBSD-style copyright and disclaimer apply

   Reflection framework
*/

#pragma once

#include <function>
#include <cstddef>

namespace slick {

struct Reflection;
struct ReflectionRegistry;


/******************************************************************************/
/* REFLECTION REGISTRY                                                        */
/******************************************************************************/

template<typename T> struct ReflectionId;

struct ReflectionRegistry
{
    template<typename T>
    static Reflection* get()
    {
        return get(ReflectionId<T>::value);
    }
    static Reflection* get(const std::string& id);

    static void add(Reflection* reflection)
};


/******************************************************************************/
/* VALUE                                                                      */
/******************************************************************************/

struct Value
{
    template<typename T>
    explicit Value(T& value) :
        value_(&value), reflection_(ReflectionRegistry::get<T>())
    {}

    Value(void* value, Reflection* reflection) :
        value_(value), reflection_(reflection)
    {}


    void* value() { return value_; }
    Reflection* reflection() { return reflection_; }

    template<typename T>
    T& cast()
    {
        assert(reflection_->isConvertibleTo<T>());
        return *reinterpret_cast<T*>(value);
    }

    template<typename T>
    void set(T&& newValue)
    {
        reflection_->set(value_, std::forward<T>(newValue));
    }

    template<typename T>
    void set(const std::string& field, T&& newValue)
    {
        reflection_->set(value_, field, std::foward<T>(newValue));
    }


    Value get()
    {
        return reflection_->get(value_);
    }

    template<typename T>
    T get()
    {
        return reflection_->get<T>(value_);
    }

    Value get(const std::string& field)
    {
        return reflection_->get(value_, field);
    }

    template<typename T>
    T get(const std::string& field)
    {
        return reflection_->get<T>(value_, field);
    }

private:
    void* value_;
    Reflection* reflection_;
};


/******************************************************************************/
/* CAST                                                                       */
/******************************************************************************/

template<typename Val, typename Target>
struct Cast
{
    static const Target& cast(const Val& value)
    {
        return value;
    }
};

template<typename Target>
struct Cast<Value, Target>
{
    static const Target& cast(Value value)
    {
        return value->cast<Target>();
    }
};

template<typename Target, typename V>
const Target& cast(V&& value)
{
    return Cast<V, Target>::cast(std::forward<V>(value));
}


/******************************************************************************/
/* GET REFLECTION                                                             */
/******************************************************************************/

template<typename T>
struct GetReflection
{
    Reflection* get() const { ReflectionRegistry::get<T>(); }
};

template<>
struct GetReflection<Value>
{
    Reflection* get() const { return value.reflection(); }
};


/******************************************************************************/
/* FUNCTION PROXY                                                             */
/******************************************************************************/

/** Allows for calls to functions using the generic Value container. It does
    this by casting the Value arguments to they're appropriate types and
    wrapping the return value into a Value object. It SHOULD also allow for
    mixed Value and non-Value.

 */
template<typename Ret, typename... Args>
struct FunctionProxy
{
    FunctionProxy(std::function<Ret(const Args&...)> fn) :
        fn(std::move(fn))
    {}

    template<typename... V>
    void operator() (V... values)
    {
        slickStaticAssert(std::is_same<void, Ret>::value);
        slickStaticAssert(sizeof...(V) == sizeof...(Args));

        fn(cast<Args>(values)...);
    }

    template<typename... V>
    Value operator() (V... values)
    {
        slickStaticAssert(!std::is_same<void, Ret>::value);
        slickStaticAssert(sizeof...(V) == sizeof...(Args));

        return Value(fn(cast<Args>(values)...));
    }

private:

    std::function<Ret(const Args&...)> fn;
};


/******************************************************************************/
/* FUNCTION                                                                   */
/******************************************************************************/

struct Function
{
    template<typename Ret, typename... Args>
    Function(std::function<Ret(const Args&...)> fn) :
        fn(*reinterpret_cast<std::function<void()>*>(&fn)),
        ret(GetReflection<Ret>::get())
    {
        reflectArgs<Args...>();
    }

    template<typename Ret, typename... Args>
    bool test() const
    {
        return
            test(GetReflection<Ret>::get(), ret) &&
            testArgs<0, Args...>();
    }

    bool test(const Function& other) const
    {
        if (args.size() != other.args.size()) return false;

        for (size_t i = 0; i < args.size(); ++i) {
            if (!args[i].isConvertibleTo(other.args[i]) &&
                    !other.args[i].isConvertibleTo(args[i]))
                return false;
        }

        return true;
    }

    template<typename... Args>
    void call(const Args&... args)
    {
        assert(test<void, Args...>());

        typedef std::function<void(const Args&...)> Fn;
        *reinterpret_cast<Fn*>(&fn)(args...);
    }

    template<typename Ret, typename... Args>
    Ret call(const Args&... args)
    {
        assert(test<Ret, Args...>());

        typedef std::function<Ret(const Args&...)> Fn;
        return *reinterpret_cast<Fn*>(&fn)(args...);
    }

    Reflection* returnType() const { return ret; }
    size_t size() const { return args.size(); }
    Reflection* operator[] (size_t i) const { return args[i]; }

    bool isGetter() const
    {
        return ret != GetReflection<void>::get() && args.empty();
    }

    bool isSetter() const
    {
        return ret == GetReflection<void>::get() && args.size() == 1;
    }

private:

    template<typename Arg>
    void reflectArgs()
    {
        args.push_back(GetReflection<Arg>::get());
    }

    template<typename Arg, typename... Rest>
    void reflectArgs()
    {
        unpackArgs<Arg>();
        unpackArgs<Rest...>();
    }

    bool test(Reflection* value, Reflection* target) const
    {
        return value->isConveritbleTo(target);
    }

    template<size_t Index, typename Arg>
    void testArgs() const
    {
        return test(GetReflection<Arg>::get(), args[Index]);
    }

    template<size_t Index, typename Arg, typename... Rest>
    void testArgs() const
    {
        if (!testArgs<Index, Arg>()) return false;
        return testArgs<Index + 1, Rest...>();
    }


    std::function<void()> fn;

    Reflection* ret;
    std::vector<Reflection*> args;
};


/******************************************************************************/
/* FUNCTIONS                                                                  */
/******************************************************************************/

struct Functions
{

    size_t size() const { return overloads.size(); }
    Function& operator[] (size_t i) const { return overloads[i]; }

    bool test(Function fn)
    {
        for (const auto& other : overloads) {
            if (fn.test(other)) return true;
        }
        return false;
    }

    template<typename Ret, typename... Args>
    bool test()
    {
        for (const auto& fn : overloads) {
            if (fn.test<Ret, Args...>()) return true;
        }
        return false;

    }

    void add(Function fn)
    {
        assert(!test(fn) && "ambiguous overload");
        overloads.push_back(fn);
    }


    template<typename... Args>
    void call(Args&&... args)
    {
        for (const auto& fn : overloads) {
            if (!fn.test<Ret, Args...>()) continue;

            fn.call(std::forward<Args>(args)...);
            return;
        }

        assert(false && "no oveloads available");
    }

    template<typename Ret, typename... Args>
    Ret call(Args&&... args)
    {
        for (const auto& fn : overloads) {
            if (!fn.test<Ret, Args...>()) continue;

            return fn.call(std::forward<Args>(args)...);
        }

        assert(false && "No oveloads available");
    }

private:
    std::vector<Function> overloads;
};


/******************************************************************************/
/* REFLECTION                                                                 */
/******************************************************************************/

struct Reflection
{
    explicit Reflection(std::string id, Reflection* parent = nullptr) :
        id(std::move(id)), parent(parent)
    {}

    Reflection(Reflection&&) = delete;
    Reflection(const Reflection&) = delete;
    Reflection& operator=(Reflection&&) = delete;
    Reflection& operator=(const Reflection&) = delete;


private:

    std::string id;
    Reflection* parent;

    std::unordered_map<std::string, Functions> fields;
};


/******************************************************************************/
/* MACROS                                                                     */
/******************************************************************************/

#define SLICK_REFLECT_NAME(_prefix_,_class_,_ns_)       \
    _prefix_ ## _ ## _ns_ ## _ ## _class_

#define SLICK_REFLECT_TYPE(_class_,_ns_)       \
    _ns_ ## :: ## _class_

#define SLICK_REFLECT_ID(_class_,_ns_)          \
    #SLICK_REFLECT_NAME(id,_class_,_ns_)


#define SLICK_REFLECT_REGISTER(_class_,_ns_)                            \
    struct SLICK_REFLECT_NAME(Register,_class_,_ns_)                    \
    {                                                                   \
        SLICK_REFLECT_NAME(Register,_class_,_ns_)()                     \
        {                                                               \
            std::string id = SLICK_REFLECT_ID(_class_,_ns_);            \
            ReflectionRegistry::add(new Reflection(id));                \
            SLICK_REFLECT_NAME(init,_class_,_ns_)();                    \
        }                                                               \
    } SLICK_REFLECT_NAME(register,_class_,_ns_);


#define SLICK_REFLECT_ID_CLASS(_class_,_ns_)                    \
    template<>                                                  \
    struct ReflectionId<SLICK_REFLECT_TYPE(_class_,_ns_)>       \
    {                                                           \
        static constexpr std::string value =                    \
            SLICK_REFLECT_ID(_class_,_ns_);                     \
    };
    

#define SLICK_REFLECT(_class_,_ns_)                                     \
    namespace slick { namespace reflect {                               \
    void SLICK_REFLECT_NAME(init,_class_,_ns_)();                       \
    SLICK_REFLECT_ID_CLASS(_class_,_ns_)                                \
    SLICK_REFLECT_REGISTER(_class_,_ns_)                                \
    }}                                                                  \
    void slick::reflect::SLICK_REFLECT_NAME(init,_class_,_ns_)()

            



} // slick
