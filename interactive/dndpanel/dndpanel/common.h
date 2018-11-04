#pragma once

#include "logger.h"

#include <chrono>
#include <memory>
#include <string>

using high_clock = std::chrono::high_resolution_clock;
using ms = std::chrono::milliseconds;

#define DECLARE_SMARTPOINTER(_TYPE_)			\
class _TYPE_;									\
/**\brief A reference counting shared pointer to _TYPE_ */ \
typedef std::shared_ptr<_TYPE_> _TYPE_##Ptr;	 \
/**\brief A reference counting weak pointer to _TYPE_ */  \
typedef std::weak_ptr<_TYPE_> _TYPE_##WeakPtr   

#define DECLARE_SMARTPOINTER_WITHOUT_DCL(_TYPE_)	\
/**\brief A reference counting shared pointer to _TYPE_ */ \
typedef std::shared_ptr<_TYPE_> _TYPE_##Ptr;		\
/**\brief A reference counting weak pointer to _TYPE_ */ \
typedef std::weak_ptr<_TYPE_> _TYPE_##WeakPtr   

class SharedFromThisVirtualBase : public std::enable_shared_from_this<SharedFromThisVirtualBase>
{
protected:
    SharedFromThisVirtualBase()
    {
    }

    virtual ~SharedFromThisVirtualBase()
    {
    }

public:
    template <typename T>
    std::shared_ptr<T> GetSharedPtr()
    {
        return std::dynamic_pointer_cast<T>(shared_from_this());
    }

    template <typename T>
    std::weak_ptr<T> GetWeakPtr()
    {
        return std::weak_ptr<T>(GetSharedPtr<T>());
    }
};

//!\brief Class simply used for virtual deriving from the base class
class SharedFromThis : public virtual SharedFromThisVirtualBase
{
public:
    SharedFromThis() : SharedFromThisVirtualBase()
    {
    }

    virtual ~SharedFromThis()
    {
    }
};