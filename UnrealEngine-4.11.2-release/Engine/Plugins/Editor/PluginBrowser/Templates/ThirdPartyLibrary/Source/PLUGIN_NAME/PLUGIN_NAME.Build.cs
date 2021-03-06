// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PLUGIN_NAME : ModuleRules
{
	public PLUGIN_NAME(TargetInfo Target)
	{
		
		PublicIncludePaths.AddRange(
			new string[] {
				"PLUGIN_NAME/Public"
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"PLUGIN_NAME/Private",
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"PLUGIN_NAMELibrary",
				"Projects"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		AddThirdPartyPrivateStaticDependencies(Target,
				"PLUGIN_NAMELibrary"
				// ... add any third party modules that your module depends on here ...
				);
	}
}
