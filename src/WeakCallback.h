
#include <functional>
#include <memory>
#include <utility>
template <typename Class, typename... Args>
class WeakCallback
{
   public:
    WeakCallback(const std::weak_ptr<Class>& obj, const std::function<void(Class*, Args...)>& function_) : object_(obj), function_(function_) {}

    void operator()(Args... args) const
    {
        std::shared_ptr<Class> ptr(object_.lock());
        if (ptr)
        {
            function_(ptr.get(), std::forward<Args>(args)...);
        }
    }

   private:
    std::weak_ptr<Class> object_;
    std::function<void(Class*, Args...)> function_;
};

template <typename Class, typename... Args>
WeakCallback<Class, Args...> makeWeakCallback(const std::shared_ptr<Class>& obj, void (Class::*func)(Args...))
{
    return WeakCallback<Class, Args...>(obj, func);
}

template <typename Class, typename... Args>
WeakCallback<Class, Args...> makeWeakCallback(const std::shared_ptr<Class>& object, void (Class::*function)(Args...) const)
{
    return WeakCallback<Class, Args...>(object, function);
}
