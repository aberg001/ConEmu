﻿
/*
Copyright (c) 2016 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#pragma once

#include <windows.h>
#include "../common/Common.h"
#include "../common/kl_parts.h"
#include "../common/CEStr.h"
#include "../common/MArray.h"


#if 0
enum CEOptionType
{
	eot_Undefined = 0, // CEOption - must not be
	eot_Simple,
	eot_Array,
	eot_Composite,
};

enum CEOptionDataType
{
	eodt_Undefined = 0, // must be defined!
	eodt_Bool,    // Simple `bool`
	eodt_String,  // has MaxLength template property
	eodt_Byte,    // Simple 'u8' ('BYTE'), may be subclassed as `enum`?
	eodt_Int,     // Simple 'i32' ('int'), stored as decimal signed integer
	eodt_UInt,    // Simple 'u32' ('UINT'), stored as decimal unsigned integer
	eodt_DWORD,   // Simple 'DWORD', stored as hexadecimal unsigned integer
};
#endif



// This does not ‘fixate’ the value, only check it
template <typename T, const T val_min, const T val_max>
bool validate_within(T& value)
{
	if ((value >= val_min) && (value <= val_max))
		return true;
	return false;
};

// Make value ‘not greater than’
template <typename T, const T val_max>
bool validate_max(T& value)
{
	if (value > val_max)
		value = val_max;
	return true;
};

// Make value ‘in range’
template <typename T, const T val_min, const T val_max>
bool validate_minmax(T& value)
{
	if (value < val_min)
		value = val_min;
	else if (value > val_max)
		value = val_max;
	return true;
};


class CEOptionBase;

class CEOptionList
{
public:
	MArray<CEOptionBase*> m_List;
public:
	CEOptionList() {};
	~CEOptionList() {};
	void Register(CEOptionBase* pOption)
	{
		_ASSERTE(this!=NULL);
		// #OPT_TODO Use std::map instead of list? Check correctness on ctor/move/init in Options
		m_List.push_back(pOption);
	};
};

extern CEOptionList* gpOptionList;


// For arrays declarations
template <typename T, int MaxSize>
struct CEOptionArrayType
{
	const int max_size = MaxSize;
	typedef T type[MaxSize];
};


// Prototype
class CEOptionBase
{
protected:
	CEStr ms_Name;
public:
	CEOptionBase(LPCWSTR OptionName)
		: ms_Name(OptionName)
	{
		// #pragma warning(disable: 4355)
		if (gpOptionList)
			gpOptionList->Register(this);
		// #pragma warning(default: 4355)
	};
	virtual ~CEOptionBase()
	{
	};

	virtual bool Load(SettingsBase* reg) = 0;
	virtual bool Save(SettingsBase* reg) const = 0;
	virtual void Reset() = 0;

	virtual LPCWSTR Name()
	{
		return ms_Name;
	};
};



// Base class
template <typename T>
class CEOptionBaseImpl : public CEOptionBase
{
public:
	T m_Value;
	const T m_Default;
protected:
	bool (*validate_fn)(T& value);
public:
	CEOptionBaseImpl(LPCWSTR OptionName, const T& Default = T(), bool (*_validate_fn)(T& value) = NULL)
		: CEOptionBase(OptionName)
		, m_Value()
		, m_Default(Default)
		, ms_Name(OptionName)
		, validate_fn(_validate_fn)
	{
		Reset();
	};

	virtual ~CEOptionBaseImpl()
	{
	};

	virtual bool Load(SettingsBase* reg) override
	{
		if (!reg)
			return false;
		T value;
		if (!reg->Load(ms_Name, &value))
			return false;
		return Set(value);
	};

	virtual bool Save(SettingsBase* reg) const override
	{
		if (!reg)
			return false;
		// #OPT_TODO Serializators doesn't return error codes
		reg->Save(ms_Name, m_Value);
		return true;
	};

	operator const T&() const
	{
		return m_Value;
	};

	virtual bool Set(T value)
	{
		if (validate_fn)
		{
			if (!validate_fn(value))
				return false;
		}
		m_Value = value;
		return true;
	};

	virtual void Reset() override
	{
		m_Value = m_Default;
	}

};




// Simple 'wchar_t[NN]' property
template <typename T, int MaxSize>
class CEOptionArray : public CEOptionBase
{
public:
	using arr_type = CEOptionArrayType<T,MaxSize>::type;
	arr_type m_Value;
protected:
	LPCWSTR (*get_name)(int index);
	void (*set_default)(arr_type& value);
public:
	CEOptionArray(LPCWSTR (*_get_name)(int index), void (*_set_default)(arr_type& value) = NULL)
		: CEOptionBase(L"")
		, get_name(_get_name)
		, set_default(_set_default)
	{
		_ASSERTE(MaxSize > 0);
		Reset();
	};

	virtual ~CEOptionArray()
	{
	};

	virtual bool Load(SettingsBase* reg) override
	{
		if (!reg || !get_name)
			return false;
		bool bAllOk = true;
		if (set_default)
			set_default(m_Value);
		for (int i = 0; i < MaxSize; i++)
		{
			LPCWSTR pszName = get_name(i);
			if (!pszName || !*pszName)
			{
				if (!set_default)
					m_Value[i] = T();
			}
			else if (!reg->Load(pszName, m_Value[i]))
			{
				bAllOk = false;
			}
		}
		return bAllOk;
	};

	virtual bool Save(SettingsBase* reg) const override
	{
		if (!reg || !get_name)
			return false;
		bool bAllOk = true;
		for (int i = 0; i < MaxSize; i++)
		{
			LPCWSTR pszName = get_name(i);
			if (pszName && *pszName)
			{
				// #OPT_TODO Serializators doesn't return error codes
				reg->Save(pszName, m_Value[i]);
				// if (was error) bAllOk = false;
			}
		}
		return bAllOk;
	};

	virtual bool Set(arr_type value)
	{
		if (m_Value == value)
			return true;
		memmove(m_Value, value, sizeof(m_Value));
		return true;
	};

	virtual void Reset() override
	{
		if (set_default)
			set_default(m_Value);
		else
			memset(m_Value, 0, sizeof(m_Value));
	};

	const T & operator[](int index) const
	{
		_ASSERTE(index>=0 && index<MaxSize);
		return m_Value[index];
	};
	T & operator[](int index)
	{
		_ASSERTE(index>=0 && index<MaxSize);
		return m_Value[index];
	};
};




// Simple `bool`
class CEOptionBool : public CEOptionBaseImpl<bool>
{
public:
	CEOptionBool(LPCWSTR OptionName, const bool Default = false, bool (*_validate_fn)(bool& value) = NULL)
		: CEOptionBaseImpl(OptionName, Default, _validate_fn)
	{
	};

	virtual ~CEOptionBool()
	{
	};
};

// Simple 'wchar_t*' property
class CEOptionString : public CEOptionBase
{
public:
	wchar_t* m_Value;
	CEStr m_Default;
public:
	CEOptionString(LPCWSTR OptionName, LPCWSTR Default = NULL)
		: CEOptionBase(OptionName)
		, m_Value()
		, m_Default(Default)
	{
		Reset();
	};

	virtual ~CEOptionString()
	{
		SafeFree(m_Value);
	};

	virtual bool Load(SettingsBase* reg) override
	{
		if (!reg)
			return false;
		wchar_t* value = NULL;
		if (!reg->Load(ms_Name, &value))
			return false;
		return Set(STD_MOVE(value));
	};

	virtual bool Save(SettingsBase* reg) const override
	{
		if (!reg)
			return false;
		// #OPT_TODO Serializators doesn't return error codes
		reg->Save(ms_Name, m_Value);
		return true;
	};

	// It ‘captures’ the `value`. Sort of `std::move` from C++11.
	virtual bool Set(const wchar_t* value)
	{
		if (m_Value == value)
			return true;
		return Set(STD_MOVE(lstrdup(value)));
	}

	// It ‘captures’ the `value`!
	virtual bool Set(wchar_t*&& value)
	{
		if (m_Value == value)
			return true;
		if (!value)
			value = lstrdup(L"");
		LPVOID p = InterlockedExchangePointer((PVOID*)&m_Value, value);
		SafeFree(p);
		value = NULL; // capturing
		return true;
	};

	virtual void Reset() override
	{
		if (m_Value && m_Default.ms_Val)
		{
			if (wcscmp(m_Value, m_Default.ms_Val) == 0)
				return;
		}
		SafeFree(m_Value);
		Set(lstrdup(m_Default));
		_ASSERTE(m_Value != NULL);
	};
};



// Simple 'wchar_t[NN]' property
template <int MaxSize>
class CEOptionStrFix : public CEOptionBase
{
public:
	wchar_t m_Value[MaxSize];
	CEStr m_Default;
public:
	CEOptionStrFix(LPCWSTR OptionName, LPCWSTR Default = NULL)
		: CEOptionBase(OptionName)
		, m_Value()
		, m_Default(Default)
	{
		Reset();
	};

	virtual ~CEOptionStrFix()
	{
	};

	virtual bool Load(SettingsBase* reg) override
	{
		if (!reg)
			return false;
		if (!reg->Load(ms_Name, m_Value, MaxSize))
			return false;
		return true;
	};

	virtual bool Save(SettingsBase* reg) const override
	{
		if (!reg)
			return false;
		// #OPT_TODO Serializators doesn't return error codes
		reg->Save(ms_Name, m_Value);
		return true;
	};

	// It ‘captures’ the `value`. Sort of `std::move` from C++11.
	virtual bool Set(const wchar_t* value)
	{
		lstrcpyn(m_Value, value ? value : L"", MaxSize);
		return true;
	};

	virtual void Reset() override
	{
		Set(m_Default.ms_Val);
	};

};


// #NEW_OPTIONS  finished here
class CEOptionStringArray : public CEOptionBase
{
protected:
	MArray<wchar_t*> Items;
	const int MaxItemCount;

public:
	CEOptionStringArray(int anMaxItemCount = -1)
		: CEOptionBase(L"")
		, MaxItemCount(anMaxItemCount)
	{
	};

	virtual ~CEOptionStringArray()
	{
		Reset();
	};

	virtual void Reset() override
	{
		INT_PTR iCount = Items.size();

		for (INT_PTR i = 0; i < iCount; i++)
		{
			wchar_t* p = Items[i];
			SafeFree(p);
		}

		Items.eraseall();
	};

	LPCWSTR Get(int index) const
	{
		if (index < 0 || index >= Items.size())
			return NULL;

		LPCWSTR pszItem = Items[index];
		if (!pszItem || !*pszItem)
		{
			_ASSERTE(pszItem && *pszItem);
			return NULL;
		}

		return pszItem;
	};

	LPCWSTR operator[](int index) const
	{
		return Get(index);
	};

	int Compare(int index, LPCWSTR asCommand)
	{
		int iCmp;

		LPCWSTR pszItem = Get(index);

		if (!pszItem || !*pszItem)
			iCmp = -1;
		else if (!asCommand || !*asCommand)
			iCmp = 1;
		else
			iCmp = lstrcmp(pszItem, asCommand);

		return iCmp;
	};

	void Add(LPCWSTR asValue, bool bFront = true)
	{
		if (!asValue || !*asValue)
		{
			_ASSERTE(asValue && *asValue);
			return;
		}

		wchar_t* p;
		INT_PTR iCount;

		// bFront==false is used during History loading,
		// no need to remove duplicates from settings
		if (bFront)
		{
			// Already on first place?
			if (!Items.empty())
			{
				p = Items[0];
				if (p && lstrcmp(p, asValue) == 0)
				{
					return;
				}
			}

			// First, remove asValue from list, if it was there already
			iCount = Items.size();
			while (iCount > 0)
			{
				p = Items[--iCount];
				if (!p || lstrcmp(p, asValue) == 0)
				{
					SafeFree(p);
					Items.erase(iCount);
				}
			}

			// Second, trim history tail to MaxItemCount
			iCount = Items.size();
			while ((iCount > 0) && (iCount >= MaxItemCount))
			{
				p = Items[--iCount];
				SafeFree(p);
				Items.erase(iCount);
			}
		}

		p = lstrdup(asValue);
		if (!p)
			return;

		if (bFront)
			Items.insert(0, p);
		else
			Items.push_back(p);
	};

	// delimiter==NULL - creates MSZ data
	// otherwise it may be, for example, L"\r\n", "|", etc.
	// returns size in BYTES!
	UINT GetData(wchar_t*& rsMSZ, LPCWSTR delimiter = NULL)
	{
		_ASSERTE(rsMSZ==NULL);
		SafeFree(rsMSZ);

		if (Items.size() <= 0)
		{
			return 0;
		}

		UINT delim_len = delimiter ? wcslen(delimiter) : 1;
		INT_PTR iCount = Items.size();
		UINT cchMax = 0;
		MArray<int> len;
		len.alloc(iCount);

		for (INT_PTR i = 0; i < iCount; i++)
		{
			// skipping empty lines!
			UINT iLen = Items[i] ? wcslen(Items[i]) : 0;
			if (iLen)
				cchMax += iLen + delim_len;
			len.push_back(iLen);
		}

		if (!cchMax)
		{
			_ASSERTE(FALSE && "Length failed for non-empty array");
			return 0;
		}

		cchMax = (cchMax + 1) * sizeof(*rsMSZ);

		rsMSZ = (wchar_t*)malloc(cchMax);
		if (!rsMSZ)
		{
			_ASSERTE(rsMSZ!=NULL);
			return 0;
		}

		wchar_t* p = rsMSZ;
		for (INT_PTR i = 0; i < iCount; i++)
		{
			int iSize = len[i];
			if (iSize)
				_wcscpy_c(p, iSize+1, Items[i]);
			p += iSize;
			if (!delimiter)
			{
				p++; // MSZZ
			}
			else if (delim_len)
			{
				_wcscpy_c(p, delim_len+1, delimiter);
				p += delim_len;
			}
		}

		*p = 0;
		_ASSERTE((p > rsMSZ) && (*(p) == 0 && *(p-1) == 0));

		return cchMax;
	};

};

// #OPT_TODO Reuse "CEOptionStringArray"
template <LPCWSTR OptionName, LPCWSTR Default = NULL>
class CEOptionStringMSZ : public CEOptionBaseImpl<wchar_t*, OptionName>
{
};

// #OPT_TODO Required for "DefaultTerminalApps"
// #OPT_TODO Reuse "CEOptionStringArray"
template <LPCWSTR OptionName, LPCWSTR Default = NULL>
class CEOptionStringDelim : public CEOptionBaseImpl<wchar_t*, OptionName>
{
};


// Simple 'u8' ('BYTE')
class CEOptionByte : public CEOptionBaseImpl<u8>
{
public:
	CEOptionByte(LPCWSTR OptionName, const u8 Default = 0, bool (*_validate_fn)(u8& value) = NULL)
		: CEOptionBaseImpl(OptionName, Default, _validate_fn)
	{
	};

	virtual ~CEOptionByte()
	{
	};
};

// Enum, stored as simple 'u8' ('BYTE')
template <typename T>
class CEOptionEnum : public CEOptionBaseImpl<T>
{
public:
	CEOptionEnum(LPCWSTR OptionName, const T Default = T(), bool (*_validate_fn)(T& value)=NULL)
		: CEOptionBaseImpl(OptionName, Default, _validate_fn)
	{
	};

	virtual ~CEOptionEnum()
	{
	};

	virtual bool Load(SettingsBase* reg) override
	{
		if (!reg)
			return false;
		u8 value;
		if (!reg->Load(ms_Name, &value))
			return false;
		return Set(reinterpret_cast<T>(value));
	};

	virtual bool Save(SettingsBase* reg) const override
	{
		if (!reg)
			return false;
		// #OPT_TODO Serializators doesn't return error codes
		reg->Save(ms_Name, (u8)m_Value);
		return true;
	};

	virtual void Reset() override
	{
		Set(Default);
	};
};

// Simple 'i32' ('int'), stored as decimal signed integer
class CEOptionInt : public CEOptionBaseImpl<i32>
{
public:
	CEOptionInt(LPCWSTR OptionName, const i32 Default = 0, bool (*_validate_fn)(i32& value) = NULL)
		: CEOptionBaseImpl(OptionName, Default, _validate_fn)
	{
	};

	virtual ~CEOptionInt()
	{
	};
};

// Simple 'u32' ('UINT'), stored as decimal unsigned integer
class CEOptionUInt : public CEOptionBaseImpl<u32>
{
public:
	CEOptionUInt(LPCWSTR OptionName, const u32 Default = 0, bool (*_validate_fn)(u32& value) = NULL)
		: CEOptionBaseImpl(OptionName, Default, _validate_fn)
	{
	};

	virtual ~CEOptionUInt()
	{
	};
};

// Simple 'DWORD', stored as hexadecimal unsigned integer
class CEOptionDWORD : public CEOptionBaseImpl<DWORD>
{
public:
	CEOptionDWORD(LPCWSTR OptionName, const DWORD Default = 0, bool (*_validate_fn)(DWORD& value)=NULL)
		: CEOptionBaseImpl(OptionName, Default, _validate_fn)
	{
	};

	virtual ~CEOptionDWORD()
	{
	};
};


// CESize
class CEOptionSize : public CEOptionBaseImpl<CESize>
{
public:
	CEOptionSize(LPCWSTR OptionName, const CESize Default = CESize(), bool (*_validate_fn)(CESize& value)=NULL)
		: CEOptionBaseImpl(OptionName, Default, _validate_fn)
	{
	};

	virtual ~CEOptionSize()
	{
	};

	CESize* operator->()
	{
		return &m_Value;
	};
};



// ComSpec
// class CEOptionComSpec : public CEOptionBase, public ConEmuComspec
#include "OptTypeComSpec.h"