// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "NativeClassHierarchy.h"
#include "KismetEditorUtilities.h"
#include "GameProjectGenerationModule.h"
#include "IProjectManager.h"
#include "HotReloadInterface.h"
#include "SourceCodeNavigation.h"
#include "IPluginManager.h"

TSharedRef<FNativeClassHierarchyNode> FNativeClassHierarchyNode::MakeFolderEntry(FName InEntryName, FString InEntryPath)
{
	TSharedRef<FNativeClassHierarchyNode> NewEntry = MakeShareable(new FNativeClassHierarchyNode());
	NewEntry->Type = ENativeClassHierarchyNodeType::Folder;
	NewEntry->Class = nullptr;
	NewEntry->EntryName = InEntryName;
	NewEntry->EntryPath = MoveTemp(InEntryPath);
	return NewEntry;
}

TSharedRef<FNativeClassHierarchyNode> FNativeClassHierarchyNode::MakeClassEntry(UClass* InClass, FName InClassModuleName, FString InClassModuleRelativePath, FString InEntryPath)
{
	TSharedRef<FNativeClassHierarchyNode> NewEntry = MakeShareable(new FNativeClassHierarchyNode());
	NewEntry->Type = ENativeClassHierarchyNodeType::Class;
	NewEntry->Class = InClass;
	NewEntry->ClassModuleName = MoveTemp(InClassModuleName);
	NewEntry->ClassModuleRelativePath = MoveTemp(InClassModuleRelativePath);
	NewEntry->EntryName = InClass->GetFName();
	NewEntry->EntryPath = MoveTemp(InEntryPath);
	return NewEntry;
}

void FNativeClassHierarchyNode::AddChild(TSharedRef<FNativeClassHierarchyNode> ChildEntry)
{
	check(Type == ENativeClassHierarchyNodeType::Folder);
	Children.Add(ChildEntry->EntryName, MoveTemp(ChildEntry));
}

FNativeClassHierarchy::FNativeClassHierarchy()
{
	PopulateHierarchy();

	// Register to be notified of module changes
	FModuleManager::Get().OnModulesChanged().AddRaw(this, &FNativeClassHierarchy::OnModulesChanged);

	// Register to be notified of hot reloads
	IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
	HotReloadSupport.OnHotReload().AddRaw(this, &FNativeClassHierarchy::OnHotReload);
}

FNativeClassHierarchy::~FNativeClassHierarchy()
{
	FModuleManager::Get().OnModulesChanged().RemoveAll(this);

	if(FModuleManager::Get().IsModuleLoaded("HotReload"))
	{
		IHotReloadInterface& HotReloadSupport = FModuleManager::GetModuleChecked<IHotReloadInterface>("HotReload");
		HotReloadSupport.OnHotReload().RemoveAll(this);
	}
}

void FNativeClassHierarchy::GetMatchingClasses(const FNativeClassHierarchyFilter& Filter, TArray<UClass*>& OutClasses) const
{
	TArray<TSharedRef<FNativeClassHierarchyNode>> NodesToSearch;
	GatherMatchingNodesForPaths(Filter.ClassPaths, NodesToSearch);

	for(const auto& NodeToSearch : NodesToSearch)
	{
		GetClassesRecursive(NodeToSearch, OutClasses, Filter.bRecursivePaths);
	}
}

void FNativeClassHierarchy::GetMatchingFolders(const FNativeClassHierarchyFilter& Filter, TArray<FString>& OutFolders) const
{
	TArray<TSharedRef<FNativeClassHierarchyNode>> NodesToSearch;
	GatherMatchingNodesForPaths(Filter.ClassPaths, NodesToSearch);

	for(const auto& NodeToSearch : NodesToSearch)
	{
		GetFoldersRecursive(NodeToSearch, OutFolders, Filter.bRecursivePaths);
	}
}

void FNativeClassHierarchy::GetClassFolders(TArray<FName>& OutClassRoots, TArray<FString>& OutClassFolders, const bool bIncludeEngineClasses, const bool bIncludePluginClasses) const
{
	static const FName EngineRootNodeName = "Classes_Engine";
	static const FName GameRootNodeName = "Classes_Game";

	for(const auto& RootNode : RootNodes)
	{
		const bool bRootNodePassesFilter = 
			(RootNode.Key == GameRootNodeName) ||								// Always include game classes
			(RootNode.Key == EngineRootNodeName && bIncludeEngineClasses) ||	// Only include engine classes if we were asked for them
			(RootNode.Key != EngineRootNodeName && bIncludePluginClasses);		// Only include plugin classes if we were asked for them

		if(bRootNodePassesFilter)
		{
			OutClassRoots.Add(RootNode.Key);
			GetFoldersRecursive(RootNode.Value.ToSharedRef(), OutClassFolders);
		}
	}
}

void FNativeClassHierarchy::GetClassesRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, TArray<UClass*>& OutClasses, const bool bRecurse)
{
	for(const auto& ChildNode : HierarchyNode->Children)
	{
		if(ChildNode.Value->Type == ENativeClassHierarchyNodeType::Class)
		{
			OutClasses.Add(ChildNode.Value->Class);
		}

		if(bRecurse)
		{
			GetClassesRecursive(ChildNode.Value.ToSharedRef(), OutClasses);
		}
	}
}

void FNativeClassHierarchy::GetFoldersRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, TArray<FString>& OutFolders, const bool bRecurse)
{
	for(const auto& ChildNode : HierarchyNode->Children)
	{
		if(ChildNode.Value->Type == ENativeClassHierarchyNodeType::Folder)
		{
			OutFolders.Add(ChildNode.Value->EntryPath);
		}

		if(bRecurse)
		{
			GetFoldersRecursive(ChildNode.Value.ToSharedRef(), OutFolders);
		}
	}
}

void FNativeClassHierarchy::GatherMatchingNodesForPaths(const TArray<FName>& InClassPaths, TArray<TSharedRef<FNativeClassHierarchyNode>>& OutMatchingNodes) const
{
	if(InClassPaths.Num() == 0)
	{
		// No paths means search all roots
		OutMatchingNodes.Reserve(RootNodes.Num());
		for(const auto& RootNode : RootNodes)
		{
			OutMatchingNodes.Add(RootNode.Value.ToSharedRef());
		}
	}
	else
	{
		for(const FName& ClassPath : InClassPaths)
		{
			TSharedPtr<FNativeClassHierarchyNode> CurrentNode;

			TArray<FString> ClassPathParts;
			ClassPath.ToString().ParseIntoArray(ClassPathParts, TEXT("/"), true);
			for(const FString& ClassPathPart : ClassPathParts)
			{
				// Try and find the node associated with this part of the path...
				const FName ClassPathPartName = *ClassPathPart;
				CurrentNode = (CurrentNode.IsValid()) ? CurrentNode->Children.FindRef(ClassPathPartName) : RootNodes.FindRef(ClassPathPartName);

				// ... bail out if we didn't find a valid node
				if(!CurrentNode.IsValid())
				{
					break;
				}
			}

			if(CurrentNode.IsValid())
			{
				OutMatchingNodes.Add(CurrentNode.ToSharedRef());
			}
		}
	}
}

void FNativeClassHierarchy::PopulateHierarchy()
{
	FAddClassMetrics AddClassMetrics;

	RootNodes.Empty();

	TSet<FName> GameModules = GetGameModules();
	TMap<FName, FName> PluginModules = GetPluginModules();

	for(TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* const CurrentClass = *ClassIt;
		AddClass(CurrentClass, GameModules, PluginModules, AddClassMetrics);
	}

	UE_LOG(LogContentBrowser, Log, TEXT("Native class hierarchy populated in %0.4f seconds. Added %d classes and %d folders."), FPlatformTime::Seconds() - AddClassMetrics.StartTime, AddClassMetrics.NumClassesAdded, AddClassMetrics.NumFoldersAdded);

	ClassHierarchyUpdatedDelegate.Broadcast();
}

void FNativeClassHierarchy::AddClassesForModule(const FName& InModuleName)
{
	FAddClassMetrics AddClassMetrics;

	// Find the class package for this module
	UPackage* const ClassPackage = FindPackage(nullptr, *(FString("/Script/") + InModuleName.ToString()));
	if(!ClassPackage)
	{
		return;
	}

	TSet<FName> GameModules = GetGameModules();
	TMap<FName, FName> PluginModules = GetPluginModules();

	TArray<UObject*> PackageObjects;
	GetObjectsWithOuter(ClassPackage, PackageObjects, false);
	for(UObject* Object : PackageObjects)
	{
		UClass* const CurrentClass = Cast<UClass>(Object);
		if(CurrentClass)
		{
			AddClass(CurrentClass, GameModules, PluginModules, AddClassMetrics);
		}
	}

	UE_LOG(LogContentBrowser, Log, TEXT("Native class hierarchy updated for '%s' in %0.4f seconds. Added %d classes and %d folders."), *InModuleName.ToString(), FPlatformTime::Seconds() - AddClassMetrics.StartTime, AddClassMetrics.NumClassesAdded, AddClassMetrics.NumFoldersAdded);

	ClassHierarchyUpdatedDelegate.Broadcast();
}

void FNativeClassHierarchy::RemoveClassesForModule(const FName& InModuleName)
{
	// Modules always exist directly under a root
	for(const auto& RootNode : RootNodes)
	{
		TSharedPtr<FNativeClassHierarchyNode> ModuleNode = RootNode.Value->Children.FindRef(InModuleName);
		if(ModuleNode.IsValid())
		{
			// Remove this module from its root
			RootNode.Value->Children.Remove(InModuleName);

			// If this module was the only child of this root, then we need to remove the root as well
			if(RootNode.Value->Children.Num() == 0)
			{
				RootNodes.Remove(RootNode.Key);
			}

			ClassHierarchyUpdatedDelegate.Broadcast();

			// We've found the module - break
			break;
		}
	}
}

void FNativeClassHierarchy::AddClass(UClass* InClass, const TSet<FName>& InGameModules, const TMap<FName, FName>& InPluginModules, FAddClassMetrics& AddClassMetrics)
{
	// Ignore deprecated and temporary classes
	if(InClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) || FKismetEditorUtilities::IsClassABlueprintSkeleton(InClass))
	{
		return;
	}

	const FName ClassModuleName = GetClassModuleName(InClass);
	if(ClassModuleName.IsNone())
	{
		return;
	}

	static const FName ModuleRelativePathMetaDataKey = "ModuleRelativePath";
	const FString& ClassModuleRelativeIncludePath = InClass->GetMetaData(ModuleRelativePathMetaDataKey);
	if(ClassModuleRelativeIncludePath.IsEmpty())
	{
		return;
	}

	// Work out which root this class should go under
	const FName RootNodeName = GetClassPathRootForModule(ClassModuleName, InGameModules, InPluginModules);

	// Work out the final path to this class within the hierarchy (which isn't the same as the path on disk)
	const FString ClassModuleRelativePath = ClassModuleRelativeIncludePath.Left(ClassModuleRelativeIncludePath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
	const FString ClassHierarchyPath = ClassModuleName.ToString() + TEXT("/") + ClassModuleRelativePath;

	// Ensure we've added a valid root node
	TSharedPtr<FNativeClassHierarchyNode>& RootNode = RootNodes.FindOrAdd(RootNodeName);
	if(!RootNode.IsValid())
	{
		RootNode = FNativeClassHierarchyNode::MakeFolderEntry(RootNodeName, TEXT("/") + RootNodeName.ToString());
		++AddClassMetrics.NumFoldersAdded;
	}

	// Split the class path and ensure we have nodes for each part
	TArray<FString> HierarchyPathParts;
	ClassHierarchyPath.ParseIntoArray(HierarchyPathParts, TEXT("/"), true);
	TSharedPtr<FNativeClassHierarchyNode> CurrentNode = RootNode;
	for(const FString& HierarchyPathPart : HierarchyPathParts)
	{
		const FName HierarchyPathPartName = *HierarchyPathPart;
		TSharedPtr<FNativeClassHierarchyNode>& ChildNode = CurrentNode->Children.FindOrAdd(HierarchyPathPartName);
		if(!ChildNode.IsValid())
		{
			ChildNode = FNativeClassHierarchyNode::MakeFolderEntry(HierarchyPathPartName, CurrentNode->EntryPath + TEXT("/") + HierarchyPathPart);
			++AddClassMetrics.NumFoldersAdded;
		}
		CurrentNode = ChildNode;
	}

	// Now add the final entry for the class
	CurrentNode->AddChild(FNativeClassHierarchyNode::MakeClassEntry(InClass, ClassModuleName, ClassModuleRelativePath, CurrentNode->EntryPath + TEXT("/") + InClass->GetName()));
	++AddClassMetrics.NumClassesAdded;
}

void FNativeClassHierarchy::AddFolder(const FString& InClassPath)
{
	bool bHasAddedFolder = false;

	// Split the class path and ensure we have nodes for each part
	TArray<FString> ClassPathParts;
	InClassPath.ParseIntoArray(ClassPathParts, TEXT("/"), true);
	TSharedPtr<FNativeClassHierarchyNode> CurrentNode;
	for(const FString& ClassPathPart : ClassPathParts)
	{
		const FName ClassPathPartName = *ClassPathPart;
		TSharedPtr<FNativeClassHierarchyNode>& ChildNode = (CurrentNode.IsValid()) ? CurrentNode->Children.FindOrAdd(ClassPathPartName) : RootNodes.FindOrAdd(ClassPathPartName);
		if(!ChildNode.IsValid())
		{
			ChildNode = FNativeClassHierarchyNode::MakeFolderEntry(ClassPathPartName, CurrentNode->EntryPath + TEXT("/") + ClassPathPart);
			bHasAddedFolder = true;
		}
		CurrentNode = ChildNode;
	}

	if(bHasAddedFolder)
	{
		ClassHierarchyUpdatedDelegate.Broadcast();
	}
}

bool FNativeClassHierarchy::GetFileSystemPath(const FString& InClassPath, FString& OutFileSystemPath) const
{
	// Split the class path into its component parts
	TArray<FString> ClassPathParts;
	InClassPath.ParseIntoArray(ClassPathParts, TEXT("/"), true);
	
	// We need to have at least two sections (a root, and a module name) to be able to resolve a file system path
	if(ClassPathParts.Num() < 2)
	{
		return false;
	}

	// Is this path using a known root?
	TSharedPtr<FNativeClassHierarchyNode> RootNode = RootNodes.FindRef(*ClassPathParts[0]);
	if(!RootNode.IsValid())
	{
		return false;
	}

	// Is this path using a known module within that root?
	TSharedPtr<FNativeClassHierarchyNode> ModuleNode = RootNode->Children.FindRef(*ClassPathParts[1]);
	if(!ModuleNode.IsValid())
	{
		return false;
	}

	// Get the base file path to the module, and then append any remaining parts of the class path (as the remaining parts mirror the file system)
	if(FSourceCodeNavigation::FindModulePath(ClassPathParts[1], OutFileSystemPath))
	{
		for(int32 PathPartIndex = 2; PathPartIndex < ClassPathParts.Num(); ++PathPartIndex)
		{
			OutFileSystemPath /= ClassPathParts[PathPartIndex];
		}
		return true;
	}

	return false;
}

bool FNativeClassHierarchy::GetClassPath(UClass* InClass, FString& OutClassPath, const bool bIncludeClassName) const
{
	const FName ClassModuleName = GetClassModuleName(InClass);
	if(ClassModuleName.IsNone())
	{
		return false;
	}

	static const FName ModuleRelativePathMetaDataKey = "ModuleRelativePath";
	const FString& ClassModuleRelativeIncludePath = InClass->GetMetaData(ModuleRelativePathMetaDataKey);
	if(ClassModuleRelativeIncludePath.IsEmpty())
	{
		return false;
	}

	TSet<FName> GameModules = GetGameModules();
	TMap<FName, FName> PluginModules = GetPluginModules();

	// Work out which root this class should go under
	const FName RootNodeName = GetClassPathRootForModule(ClassModuleName, GameModules, PluginModules);

	// Work out the final path to this class within the hierarchy (which isn't the same as the path on disk)
	const FString ClassModuleRelativePath = ClassModuleRelativeIncludePath.Left(ClassModuleRelativeIncludePath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
	OutClassPath = FString(TEXT("/")) + RootNodeName.ToString() + TEXT("/") + ClassModuleName.ToString();

	if(!ClassModuleRelativePath.IsEmpty())
	{
		OutClassPath += TEXT("/") + ClassModuleRelativePath;
	}

	if(bIncludeClassName)
	{
		OutClassPath += TEXT("/") + InClass->GetName();
	}

	return true;
}

void FNativeClassHierarchy::OnModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason)
{
	switch(InModuleChangeReason)
	{
	case EModuleChangeReason::ModuleLoaded:
		AddClassesForModule(InModuleName);
		break;

	case EModuleChangeReason::ModuleUnloaded:
		RemoveClassesForModule(InModuleName);
		break;

	default:
		break;
	}
}

void FNativeClassHierarchy::OnHotReload(bool bWasTriggeredAutomatically)
{
	PopulateHierarchy();
}

FName FNativeClassHierarchy::GetClassModuleName(UClass* InClass)
{
	UPackage* const ClassPackage = InClass->GetOuterUPackage();

	if(ClassPackage)
	{
		return FPackageName::GetShortFName(ClassPackage->GetFName());
	}

	return NAME_None;
}

FName FNativeClassHierarchy::GetClassPathRootForModule(const FName& InModuleName, const TSet<FName>& InGameModules, const TMap<FName, FName>& InPluginModules)
{
	static const FName EngineRootNodeName = "Classes_Engine";
	static const FName GameRootNodeName = "Classes_Game";

	// Work out which root this class should go under (anything that isn't a game or plugin module goes under engine)
	FName RootNodeName = EngineRootNodeName;
	if(InGameModules.Contains(InModuleName))
	{
		RootNodeName = GameRootNodeName;
	}
	else if(InPluginModules.Contains(InModuleName))
	{
		const FName PluginName = InPluginModules.FindRef(InModuleName);
		RootNodeName = FName(*(FString(TEXT("Classes_")) + PluginName.ToString()));
	}

	return RootNodeName;
}

TSet<FName> FNativeClassHierarchy::GetGameModules()
{
	FGameProjectGenerationModule& GameProjectModule = FGameProjectGenerationModule::Get();

	// Build up a set of known game modules - used to work out which modules populate Classes_Game
	TSet<FName> GameModules;
	if(GameProjectModule.ProjectHasCodeFiles())
	{
		TArray<FModuleContextInfo> GameModulesInfo = GameProjectModule.GetCurrentProjectModules();
		for(const auto& GameModuleInfo : GameModulesInfo)
		{
			GameModules.Add(FName(*GameModuleInfo.ModuleName));
		}
	}

	return GameModules;
}

TMap<FName, FName> FNativeClassHierarchy::GetPluginModules()
{
	IPluginManager& PluginManager = IPluginManager::Get();

	// Build up a map of plugin modules -> plugin names - used to work out which modules populate a given Classes_PluginName
	TMap<FName, FName> PluginModules;
	{
		TArray<FPluginStatus> Plugins = PluginManager.QueryStatusForAllPlugins();
		for(const FPluginStatus& Plugin: Plugins)
		{
			for(const FModuleDescriptor& PluginModule: Plugin.Descriptor.Modules)
			{
				PluginModules.Add(PluginModule.Name, FName(*Plugin.Name));
			}
		}
	}

	return PluginModules;
}