// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class DetectProjectSettingsTask : IModalTask, IDisposable
	{
		public PerforceConnection PerforceClient;
		public string NewSelectedFileName;
		public string NewSelectedProjectIdentifier;
		public string NewProjectEditorTarget;
		public string BranchClientPath;
		public string BaseEditorTargetPath;
		public string NewSelectedClientFileName;
		public Image ProjectLogo;
		public TimeSpan ServerTimeZone;
		TextWriter Log;

		public DetectProjectSettingsTask(string InNewSelectedFileName, TextWriter InLog)
		{
			NewSelectedFileName = InNewSelectedFileName;
			Log = InLog;
		}

		public void Dispose()
		{
			if(ProjectLogo != null)
			{
				ProjectLogo.Dispose();
				ProjectLogo = null;
			}
		}

		public bool Run(out string ErrorMessage)
		{
			PerforceConnection Perforce = new PerforceConnection(null, null, null);

			// Get the Perforce server info
			PerforceInfoRecord PerforceInfo;
			if(!Perforce.Info(out PerforceInfo, Log))
			{
				ErrorMessage = String.Format("Couldn't get Perforce server info");
				return false;
			}
			if(String.IsNullOrEmpty(PerforceInfo.UserName))
			{
				ErrorMessage = "Missing user name in call to p4 info";
				return false;
			}
			if(String.IsNullOrEmpty(PerforceInfo.HostName))
			{
				ErrorMessage = "Missing host name in call to p4 info";
				return false;
			}
			ServerTimeZone = PerforceInfo.ServerTimeZone;

			// Find all the clients on this machine
			Log.WriteLine("Enumerating clients on local machine...");
			List<PerforceClientRecord> Clients;
			if(!Perforce.FindClients(out Clients, Log))
			{
				ErrorMessage = String.Format("Couldn't find any clients for this host.");
				return false;
			}

			// Find any clients which are valid. If this is not exactly one, we should fail.
			List<PerforceConnection> CandidateClients = new List<PerforceConnection>();
			foreach(PerforceClientRecord Client in Clients)
			{
				// Make sure the client is well formed
				if(!String.IsNullOrEmpty(Client.Name) && (!String.IsNullOrEmpty(Client.Host) || !String.IsNullOrEmpty(Client.Owner)) && !String.IsNullOrEmpty(Client.Root))
				{
					// Require either a username or host name match
					if((String.IsNullOrEmpty(Client.Host) || String.Compare(Client.Host, PerforceInfo.HostName, StringComparison.InvariantCultureIgnoreCase) == 0) && (String.IsNullOrEmpty(Client.Owner) || String.Compare(Client.Owner, PerforceInfo.UserName, StringComparison.InvariantCultureIgnoreCase) == 0))
					{
						if(!Utility.SafeIsFileUnderDirectory(NewSelectedFileName, Client.Root))
						{
							Log.WriteLine("Rejecting {0} due to root mismatch ({1})", Client.Name, Client.Root);
							continue;
						}

						PerforceConnection CandidateClient = new PerforceConnection(PerforceInfo.UserName, Client.Name, Perforce.ServerAndPort);

						bool bFileExists;
						if(!CandidateClient.FileExists(NewSelectedFileName, out bFileExists, Log) || !bFileExists)
						{
							Log.WriteLine("Rejecting {0} due to file not existing in workspace", Client.Name);
							continue;
						}

						List<PerforceFileRecord> Records;
						if(!CandidateClient.Stat(NewSelectedFileName, out Records, Log))
						{
							Log.WriteLine("Rejecting {0} due to {1} not in depot", Client.Name, NewSelectedFileName);
							continue;
						}

						Records.RemoveAll(x => !x.IsMapped);
						if(Records.Count != 1)
						{
							Log.WriteLine("Rejecting {0} due to {1} matching records", Client.Name, Records.Count);
							continue;
						}

						Log.WriteLine("Found valid client {0}", Client.Name);
						CandidateClients.Add(CandidateClient);
					}
				}
			}

			// Check there's only one client
			if(CandidateClients.Count == 0)
			{
				ErrorMessage = String.Format("Couldn't find any Perforce workspace containing {0}.", NewSelectedFileName);
				return false;
			}
			else if(CandidateClients.Count > 1)
			{
				ErrorMessage = String.Format("Found multiple workspaces containing {0}:\n\n{1}\n\nCannot determine which to use.", Path.GetFileName(NewSelectedFileName), String.Join("\n", CandidateClients.Select(x => x.ClientName)));
				return false;
			}

			// Take the client we've chosen
			PerforceClient = CandidateClients[0];

			// Get the client path for the project file
			if(!PerforceClient.ConvertToClientPath(NewSelectedFileName, out NewSelectedClientFileName, Log))
			{
				ErrorMessage = String.Format("Couldn't get client path for {0}", NewSelectedFileName);
				return false;
			}

			// Figure out where the engine is in relation to it
			for(int EndIdx = NewSelectedClientFileName.Length - 1;;EndIdx--)
			{
				if(EndIdx < 2)
				{
					ErrorMessage = String.Format("Could not find engine in Perforce relative to project path ({0})", NewSelectedClientFileName);
					return false;
				}
				if(NewSelectedClientFileName[EndIdx] == '/')
				{
					bool bFileExists;
					if(!PerforceClient.FileExists(NewSelectedClientFileName.Substring(0, EndIdx) + "/Engine/Source/UE4Editor.target.cs", out bFileExists, Log))
					{
						ErrorMessage = String.Format("Could not find engine in Perforce relative to project path ({0})", NewSelectedClientFileName);
						return false;
					}
					else if(bFileExists)
					{
						BranchClientPath = NewSelectedClientFileName.Substring(0, EndIdx);
						break;
					}
				}
			}
			Log.WriteLine("Found branch root at {0}", BranchClientPath);

			// Get the local branch root
			if(!PerforceClient.ConvertToLocalPath(BranchClientPath + "/Engine/Source/UE4Editor.target.cs", out BaseEditorTargetPath, Log))
			{
				ErrorMessage = String.Format("Couldn't get local path for editor target file");
				return false;
			}

			// Find the editor target for this project
			if(NewSelectedFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
			{
				List<PerforceFileRecord> Files;
				if(PerforceClient.FindFiles(PerforceUtils.GetClientOrDepotDirectoryName(NewSelectedClientFileName) + "/Source/*Editor.Target.cs", out Files, Log) && Files.Count >= 1)
				{
					string DepotPath = Files[0].DepotPath;
					NewProjectEditorTarget = Path.GetFileNameWithoutExtension(Path.GetFileNameWithoutExtension(DepotPath.Substring(DepotPath.LastIndexOf('/') + 1)));
					Log.WriteLine("Using {0} as editor target name (from {1})", NewProjectEditorTarget, Files[0]);
				}
				else
				{
					Log.WriteLine("Couldn't find any editor targets for this project.");
				}
			}

			// Get a unique name for the project that's selected. For regular branches, this can be the depot path. For streams, we want to include the stream name to encode imports.
			string StreamName;
			if(PerforceClient.GetActiveStream(out StreamName, Log))
			{
				string ExpectedPrefix = String.Format("//{0}/", PerforceClient.ClientName);
				if(!NewSelectedClientFileName.StartsWith(ExpectedPrefix, StringComparison.InvariantCultureIgnoreCase))
				{
					ErrorMessage = String.Format("Unexpected client path; expected '{0}' to begin with '{1}'", NewSelectedClientFileName, ExpectedPrefix);
					return false;
				}
				NewSelectedProjectIdentifier = String.Format("{0}/{1}", StreamName, NewSelectedClientFileName.Substring(ExpectedPrefix.Length));
			}
			else
			{
				if(!PerforceClient.ConvertToDepotPath(NewSelectedClientFileName, out NewSelectedProjectIdentifier, Log))
				{
					ErrorMessage = String.Format("Couldn't get depot path for {0}", NewSelectedFileName);
					return false;
				}
			}

			// Read the project logo
			if(NewSelectedFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
			{
				string LogoFileName = Path.Combine(Path.GetDirectoryName(NewSelectedFileName), "Build", "UnrealGameSync.png");
				if(File.Exists(LogoFileName))
				{
					try
					{
						// Duplicate the image, otherwise we'll leave the file locked
						using(Image Image = Image.FromFile(LogoFileName))
						{
							ProjectLogo = new Bitmap(Image);
						}
					}
					catch
					{
						ProjectLogo = null;
					}
				}
			}

			// Succeeed!
			ErrorMessage = null;
			return true;
		}
	}
}
