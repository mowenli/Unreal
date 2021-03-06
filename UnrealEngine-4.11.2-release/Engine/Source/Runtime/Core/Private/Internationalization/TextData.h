// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITextData.h"
#include "TextHistory.h"

/** 
 * Implementation of ITextData that stores the common data and functionality.
 */
template <typename THistoryType>
class TTextData : public ITextData
{
public:
	TTextData()
		: LocalizedString()
		, History()
	{
	}

	explicit TTextData(FTextDisplayStringPtr InLocalizedString)
		: LocalizedString(MoveTemp(InLocalizedString))
		, History()
	{
	}

	TTextData(FTextDisplayStringPtr InLocalizedString, THistoryType InHistory)
		: LocalizedString(MoveTemp(InLocalizedString))
		, History(MoveTemp(InHistory))
	{
	}

	virtual ~TTextData()
	{
	}

	virtual FTextDisplayStringPtr GetLocalizedString() const override
	{
		return LocalizedString;
	}

	virtual FTextDisplayStringPtr& GetMutableLocalizedString() override
	{
		return LocalizedString;
	}

	virtual const FTextHistory& GetTextHistory() const override
	{
		return History;
	}

	virtual FTextHistory& GetMutableTextHistory() override
	{
		return History;
	}

protected:
	FTextDisplayStringPtr LocalizedString;
	THistoryType History;
};


/** 
 * Implementation of ITextData optimized to track localized text retrieved from the text localization manager, or (re)generated via persistent text history.
 */
template <typename THistoryType>
class TLocalizedTextData : public TTextData<THistoryType>
{
public:
	TLocalizedTextData()
	{
	}

	explicit TLocalizedTextData(FTextDisplayStringRef InLocalizedString)
		: TTextData<THistoryType>(InLocalizedString)
	{
	}

	TLocalizedTextData(FTextDisplayStringRef InLocalizedString, THistoryType InHistory)
		: TTextData<THistoryType>(InLocalizedString, MoveTemp(InHistory))
	{
	}

	virtual ~TLocalizedTextData()
	{
	}

	virtual const FString& GetDisplayString() const override
	{
		return *this->LocalizedString;
	}

	virtual void PersistText() override
	{
	}
};


/**
 * Implementation of ITextData optimized to track text that was generated at runtime.
 * This data will avoid heap allocating its localized string until we know that it needs to be persisted.
 */
template <typename THistoryType>
class TGeneratedTextData : public TTextData<THistoryType>
{
public:
	TGeneratedTextData()
	{
	}

	explicit TGeneratedTextData(FString InDisplayString)
		: TTextData<THistoryType>(nullptr)
		, DisplayString(MoveTemp(InDisplayString))
	{
	}

	TGeneratedTextData(FString InDisplayString, THistoryType InHistory)
		: TTextData<THistoryType>(nullptr, MoveTemp(InHistory))
		, DisplayString(MoveTemp(InDisplayString))
	{
	}

	virtual ~TGeneratedTextData()
	{
	}

	virtual const FString& GetDisplayString() const override
	{
		return (this->LocalizedString.IsValid()) ? *this->LocalizedString : DisplayString;
	}

	virtual void PersistText() override
	{
		if (!this->LocalizedString.IsValid())
		{
			FScopeLock ScopeLock(&PersistTextCS);

			// Check the pointer again in case another thread beat us to it
			if (!this->LocalizedString.IsValid())
			{
				// We copy (rather than move) DisplayString here, as other threads may currently be accessing it
				this->LocalizedString = MakeShareable(new FString(DisplayString));
			}
		}
	}
    
protected:
	FString DisplayString;
	FCriticalSection PersistTextCS;
};
