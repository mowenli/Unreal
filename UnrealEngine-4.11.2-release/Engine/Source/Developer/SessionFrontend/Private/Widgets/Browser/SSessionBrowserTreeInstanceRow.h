// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SSessionBrowserTreeRow"


/**
 * Implements a row widget for the session browser tree.
 */
class SSessionBrowserTreeInstanceRow
	: public SMultiColumnTableRow<TSharedPtr<FSessionBrowserTreeItem>>
{
public:

	SLATE_BEGIN_ARGS(SSessionBrowserTreeInstanceRow) { }
		SLATE_ATTRIBUTE(FText, HighlightText)
		SLATE_ARGUMENT(TSharedPtr<STableViewBase>, OwnerTableView)
		SLATE_ARGUMENT(TSharedPtr<FSessionBrowserInstanceTreeItem>, Item)
		SLATE_ARGUMENT(bool, ShowSelection)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		HighlightText = InArgs._HighlightText;
		Item = InArgs._Item;

		SMultiColumnTableRow<TSharedPtr<FSessionBrowserTreeItem>>::Construct(FSuperRowType::FArguments().ShowSelection(InArgs._ShowSelection).Style(FEditorStyle::Get(), "TableView.Row"), InOwnerTableView);
	}

public:

	/**
	 * Generates the widget for the specified column.
	 *
	 * @param ColumnName The name of the column to generate the widget for.
	 * @return The widget.
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == "Device")
		{
			ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

			if (InstanceInfo.IsValid())
			{
				const PlatformInfo::FPlatformInfo* const PlatformInfo = PlatformInfo::FindPlatformInfo(*InstanceInfo->GetPlatformName());

				return SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
							.WidthOverride(24)
							.HeightOverride(24)
							[
								SNew(SImage)
									.Image((PlatformInfo) ? FEditorStyle::GetBrush(PlatformInfo->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)) : FStyleDefaults::GetNoBrush())
							]
					]

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(2.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.ColorAndOpacity(this, &SSessionBrowserTreeInstanceRow::HandleTextColorAndOpacity)
							.Text(this, &SSessionBrowserTreeInstanceRow::HandleDeviceColumnText)
					];
			}
		}
		else if (ColumnName == "Level")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0, 0.0))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SSessionBrowserTreeInstanceRow::HandleTextColorAndOpacity)
						.Text(this, &SSessionBrowserTreeInstanceRow::HandleLevelColumnText)
				];
		}
		else if (ColumnName == "Name")
		{
			ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

			if (InstanceInfo.IsValid())
			{
				return SNew(SBox)
					.Padding(FMargin(1.0f, 1.0f, 4.0f, 1.0f))
					.HAlign(HAlign_Left)
					[
						SNew(SBorder)
							.BorderBackgroundColor(this, &SSessionBrowserTreeInstanceRow::HandleInstanceBorderBackgroundColor)
							.BorderImage(this, &SSessionBrowserTreeInstanceRow::HandleInstanceBorderBrush)
							.ColorAndOpacity(FLinearColor(0.25f, 0.25f, 0.25f))
							.Padding(FMargin(6.0f, 4.0f))
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Font(FEditorStyle::GetFontStyle("BoldFont"))
									.Text(FText::FromString(InstanceInfo->GetInstanceName()))
							]
					];
			}
		}
		else if (ColumnName == "Status")
		{
			return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
						.Image(this, &SSessionBrowserTreeInstanceRow::HandleAuthorizedImage)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(2.0f, 0.0f, 0.0f, 4.0f))
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
						.Image(this, &SSessionBrowserTreeInstanceRow::HandleStatusImage)
				];
		}
		else if (ColumnName == "Type")
		{
			ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

			if (InstanceInfo.IsValid())
			{
				return SNew(SBox)
					.Padding(FMargin(4.0f, 0.0f))
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.ColorAndOpacity(this, &SSessionBrowserTreeInstanceRow::HandleTextColorAndOpacity)
							.Text(this, &SSessionBrowserTreeInstanceRow::HandleInstanceTypeText)
					];
			}
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	/** Callback for getting the image of the Authorized icon. */
	const FSlateBrush* HandleAuthorizedImage() const
	{
		ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

		if (InstanceInfo.IsValid() && !InstanceInfo->IsAuthorized())
		{
			return FEditorStyle::GetBrush("SessionBrowser.SessionLocked");
		}

		return nullptr;
	}

	/** Callback for getting the text in the 'Device' column. */
	FText HandleDeviceColumnText() const
	{
		ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

		if (InstanceInfo.IsValid())
		{
			return FText::FromString(InstanceInfo->GetDeviceName());
		}

		return FText::GetEmpty();
	}

	/** Callback for getting the border color for this row. */
	FSlateColor HandleInstanceBorderBackgroundColor() const
	{
		ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

		if (InstanceInfo.IsValid())
		{
			return FLinearColor((GetTypeHash(InstanceInfo->GetInstanceId()) & 0xff) * 360.0 / 256.0, 0.8, 0.3, 1.0).HSVToLinearRGB();
		}

		return FLinearColor::Transparent;
	}

	/** Callback for getting the border brush for this row. */
	const FSlateBrush* HandleInstanceBorderBrush() const
	{
		ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

		if (InstanceInfo.IsValid())
		{
			if (FDateTime::UtcNow() - InstanceInfo->GetLastUpdateTime() < FTimespan::FromSeconds(10.0))
			{
				return FEditorStyle::GetBrush("ErrorReporting.Box");
			}

			return FEditorStyle::GetBrush("ErrorReporting.EmptyBox");
		}

		return nullptr;
	}

	/** Callback for getting the type of the session instance. */
	FText HandleInstanceTypeText() const
	{
		ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

		if (InstanceInfo.IsValid())
		{
			return FText::FromString(InstanceInfo->GetInstanceType());
		}

		return FText::GetEmpty();
	}

	/** Callback for getting the instance's current level. */
	FText HandleLevelColumnText() const
	{
		ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

		if (InstanceInfo.IsValid())
		{
			return FText::FromString(InstanceInfo->GetCurrentLevel());
		}

		return FText::GetEmpty();
	}

	/** Callback for getting the image of the Status icon. */
	const FSlateBrush* HandleStatusImage() const
	{
		ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

		if (InstanceInfo.IsValid())
		{
			if (FDateTime::UtcNow() - InstanceInfo->GetLastUpdateTime() < FTimespan::FromSeconds(10.0))
			{
				return FEditorStyle::GetBrush("SessionBrowser.StatusRunning");
			}

			return FEditorStyle::GetBrush("SessionBrowser.StatusTimedOut");
		}

		return nullptr;
	}

	/** Callback for getting the foreground text color. */
	FSlateColor HandleTextColorAndOpacity() const
	{
		ISessionInstanceInfoPtr InstanceInfo = Item->GetInstanceInfo();

		if (InstanceInfo.IsValid())
		{
			if (FDateTime::UtcNow() - InstanceInfo->GetLastUpdateTime() < FTimespan::FromSeconds(10.0))
			{
				return FSlateColor::UseForeground();
			}
		}

		return FSlateColor::UseSubduedForeground();
	}

private:

	/** The highlight string for this row. */
	TAttribute<FText> HighlightText;

	/** A reference to the tree item that is displayed in this row. */
	TSharedPtr<FSessionBrowserInstanceTreeItem> Item;
};


#undef LOCTEXT_NAMESPACE
