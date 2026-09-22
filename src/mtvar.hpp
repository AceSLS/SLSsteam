#pragma once

#include <memory>
#include <mutex>
#include <shared_mutex>


template<typename T> class MTVariable
{
private:
	std::shared_ptr<const T> ptr;
	std::shared_mutex mutex;

public:
	MTVariable()
	{
		set(defaultInst());
	}

	MTVariable(const T& instance)
	{
		set(instance);
	}

	MTVariable(const MTVariable<T>& rhs)
	{
		ptr = rhs.ptr;
	}

	constexpr T defaultInst() const
	{
		return T();
	}

	constexpr const T copy()
	{
		const auto lock = std::shared_lock(mutex);
		//Return *ptr instead of *get() to skip a useless shared_ptr con- & destruction
		return *ptr;
	}

	std::shared_ptr<const T> get()
	{
		const auto lock = std::shared_lock(mutex);
		return ptr;
	}

	void set(const T& value)
	{
		const auto newPtr = std::make_shared<const T>(value);
		const auto lock = std::lock_guard(mutex);
		ptr = newPtr;
	}

	constexpr void operator=(const T& val)
	{
		set(val);
	}

	constexpr void operator=(MTVariable<T>& other)
	{
		const auto pVal = other.get();
		set(*pVal);
	}
};
