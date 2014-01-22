/////////////////////////////////////////////////////////////////////////////////
//
// Aurora C++ Library
// Copyright (c) 2012-2014 Jan Haller
// 
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 
// 3. This notice may not be removed or altered from any source distribution.
//
/////////////////////////////////////////////////////////////////////////////////

// Some notes on derived-to-base conversion and why an additional indirection object (PtrIndirection) is necessary:
// PtrOwnerBase must be a template of T. If it were not (like the reference counter in std::shared_ptr), then there would be no way to extract the
// T* pointer of a PtrOwner [1], which is required after copying the PtrOwner. However, as long as PtrOwnerBase<T> is a template, no two CopiedPtr<X>
// with different X can refer to the same type-erased base class template PtrOwnerBase. Thus, the PtrOwner implementation would have to create a
// PtrOwner<U> with U != T, and because PtrOwner does not know both U and T [2], this is impossible.
//
// [1] There is no way of passing static type information across concrete derived implementation classes, when only the type-erased base is known.
// shared_ptr doesn't need this extraction, because it has a direct pointer; however CopiedPtr creates a *new* pointer that is unknown.
// [2] A CopiedPtr<Base> that is constructed from CopiedPtr<Derived> has no type information about the dynamic type held by the latter.
// The dynamic type needn't be Derived, also VeryDerived (which inherits Derived) is possible -- through a previous CopiedPtr<Derived> constructor call
// to either CopiedPtr(VeryDerived*), CopiedPtr(Derived*) or CopiedPtr(const CopiedPtr<VeryDerived>&). Note that in the 2nd case, the CopiedPtr has never
// had any static type information, the user upcast the pointer already before passing it to the constructor. But even in the 1st and 3rd case, the
// static type information is type-erased and only available within a derived object like PtrOwner.
// 
// Conclusion: In the general case, it is not possible to maintain a single type-erased object for the implementation of derived-to-base conversions.
// Some special cases could be optimized using RTTI trial and error, but that's just a hack which doesn't solve the inherent underlying problem.
// The current implementation is very clean: No static_cast, dynamic_cast or typeid is used.


#ifndef AURORA_PTROWNER_HPP
#define AURORA_PTROWNER_HPP

#include <Aurora/Tools/NonCopyable.hpp>

#include <cassert>


namespace aurora
{
namespace detail
{
	
	// Types to differ between move and copy semantics
	struct CopyTag {};
	struct MoveTag {};

	// Abstract base class for pointer owners
	template <typename T>
	struct PtrOwnerBase
	{
		virtual ~PtrOwnerBase() {}

		// Returns an independent polymorphic copy
		virtual PtrOwnerBase<T>*	clone() const = 0;

		// Returns the stored pointer
		virtual T*					getPointer() const = 0;
	};


	// Default pointer owner
	template <typename T, typename U, typename C, typename D>
	struct PtrOwner : PtrOwnerBase<T>
	{
		PtrOwner(U* pointer, C cloner, D deleter)
		: pointer(pointer)
		, cloner(cloner)
		, deleter(deleter)
		{
		}

		virtual ~PtrOwner()
		{
			if (pointer)
				deleter(pointer);
		}

		virtual PtrOwner* clone() const
		{
			assert(pointer);
			return new PtrOwner(cloner(pointer), cloner, deleter);
		}

		virtual T* getPointer() const
		{
			return pointer;
		}

		U* pointer;
		C cloner;
		D deleter;
	};
	
	// Used for U* -> T* derived-to-base conversion
	// See notes at the beginning of the document
	template <typename T, typename U>
	struct PtrIndirection : PtrOwnerBase<T>
	{
		explicit PtrIndirection(const PtrOwnerBase<U>* originBase, CopyTag)
		: base(originBase->clone())
		{
		}

		explicit PtrIndirection(PtrOwnerBase<U>* sourceBase, MoveTag)
		: base(sourceBase)
		{
		}

		virtual ~PtrIndirection()
		{
			delete base;
		}

		virtual PtrIndirection<T, U>* clone() const
		{
			return new PtrIndirection<T, U>(base, CopyTag());
		}

		virtual T* getPointer() const
		{
			return base->getPointer();
		}

		PtrOwnerBase<U>* base;
	};
	
	// Maker (object generator) idiom for PtrOwner
	template <typename T, typename U, typename C, typename D>
	PtrOwnerBase<T>* newPtrOwner(U* pointer, C cloner, D deleter)
	{
		return new PtrOwner<T, U, C, D>(pointer, cloner, deleter);
	}

} // namespace detail
} // namespace aurora

#endif // AURORA_PTROWNER_HPP
