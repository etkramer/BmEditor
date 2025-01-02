/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
 #include "Stdafx.h"
#include "AndroidSymbolParser.h"
#include <stdio.h>
#include <string>

using namespace System::IO;
using namespace System::Text;

namespace StandaloneSymbolParser
{
	/**
	 * Constructor.
	 */
	AndroidSymbolParser::AndroidSymbolParser() : CmdLineProcess(new FCommandLineWrapper())
	{

	}

	/**
	 * Converted to virtual void Dispose().
	 */
	AndroidSymbolParser::~AndroidSymbolParser()
	{
		this->!AndroidSymbolParser();
		GC::SuppressFinalize(this);
	}

	/**
	 * Finalizer frees native memory.
	 */
	AndroidSymbolParser::!AndroidSymbolParser()
	{
		if(CmdLineProcess)
		{
			delete CmdLineProcess;
			CmdLineProcess = NULL;
		}
	}

	/**
	 * Loads symbols for an executable.
	 *
	 * @param	ExePath		The path to the executable whose symbols are going to be loaded.
	 * @param	Modules		The list of modules loaded by the process.
	 */
	bool AndroidSymbolParser::LoadSymbols(String ^ExePath, array<ModuleInfo^> ^/*Modules*/)
	{
// 		// convert .apk to .symbols.so
// 		if(ExePath->EndsWith(L".apk"))
// 		{
// 			ExePath = ExePath->Replace(".apk", ".symbols.so");
// 		}
// 
// 
// 		// make sure the file exists
// 		if(!File::Exists(ExePath))
		{
			System::Windows::Forms::OpenFileDialog ^OpenFileDlg = gcnew System::Windows::Forms::OpenFileDialog();

			OpenFileDlg->Title = L"Find .so file for the running Android game";
			OpenFileDlg->Filter = L".so Files|*.so";
			OpenFileDlg->RestoreDirectory = true;
			OpenFileDlg->DefaultExt = "so";
			OpenFileDlg->AddExtension = true;

			if(OpenFileDlg->ShowDialog() == System::Windows::Forms::DialogResult::OK)
			{
				ExePath = OpenFileDlg->FileName;
			}
			else
			{
				return false;
			}
		}

		// cache the exe path
		ExecutablePath = ExePath;

		// hunt down the addr2line executable (allowing for an environment variable to help)
		String^ AndroidRoot = Environment::GetEnvironmentVariable("ANDROID_ROOT");
		if (AndroidRoot == nullptr)
		{
			AndroidRoot = L"C:\\Android";
			if (!File::Exists(L"C:\\Android") && File::Exists(L"D:\\Android"))
			{
				AndroidRoot = L"D:\\Android";
			}
		}

		// get NDK directory
		String^ NDKRoot = Environment::GetEnvironmentVariable("NDKROOT");
		if (NDKRoot == nullptr)
		{

			// create NDKRoot
			NDKRoot = AndroidRoot + L"\\android-ndk-r4b";
		}

		if (Environment::GetEnvironmentVariable("CYGWIN") == nullptr)
		{
			Environment::SetEnvironmentVariable("CYGWIN", "nodosfilewarning");
		}
		if (Environment::GetEnvironmentVariable("CYGWIN_HOME") == nullptr)
		{
			Environment::SetEnvironmentVariable("CYGWIN_HOME", Path::Combine(AndroidRoot, "cygwin"));
		}

		// put cygwin/bin into the path
		Environment::SetEnvironmentVariable("PATH", String::Format("{0};{1}/bin",
			Environment::GetEnvironmentVariable("PATH"),
			Environment::GetEnvironmentVariable("CYGWIN_HOME")));

		// generate the commandline for interactive addr2line
		String^ CmdLine = String::Format(L"{0}\\build\\prebuilt\\windows\\arm-eabi-4.4.0\\bin\\arm-eabi-addr2line.exe -fCe {1}", NDKRoot, ExecutablePath);

		pin_ptr<Byte> NativeCmdLine = &Encoding::UTF8->GetBytes(CmdLine)[0];

		// attempt to create it; if it fails, then we do arm-eabi-addr2line in ResolveAddressToSymboInfo with one call per line
		CmdLineProcess->Create((char*)NativeCmdLine);

		// if we got here, we are good to go
		return true;
	}

	/**
	 * Unloads any currently loaded symbols.
	 */
	void AndroidSymbolParser::UnloadSymbols()
	{
		CmdLineProcess->Terminate();
	}

	/**
	 * Retrieves the symbol info for an address.
	 *
	 * @param	Address			The address to retrieve the symbol information for.
	 * @param	OutFileName		The file that the instruction at Address belongs to.
	 * @param	OutFunction		The name of the function that owns the instruction pointed to by Address.
	 * @param	OutLineNumber	The line number within OutFileName that is represented by Address.
	 * @return	True if the function succeeds.
	 */
	bool AndroidSymbolParser::ResolveAddressToSymboInfo(unsigned __int64 Address, String ^%OutFileName, String ^%OutFunction, int %OutLineNumber)
	{
		// if we didn't already create a addr2line process, there's a problem
		if (!CmdLineProcess->IsCreated())
		{
			System::Windows::Forms::MessageBox::Show(nullptr, L"Failed to create arm-eabi-addr2line process. Make sure the Android tools are installed as expected.", L"Process creation error", System::Windows::Forms::MessageBoxButtons::OK, System::Windows::Forms::MessageBoxIcon::Error);
			return false;
		}

		// send an address to addr2line
		char Temp[128];
		sprintf_s(Temp, 128, "0x%I64x", Address);

		CmdLineProcess->Write(Temp);
		CmdLineProcess->Write("\n");

		// now read the output; sample:
		// UEngine::Exec(char const*, FOutputDevice&)
		// d:\dev\SomeFile.cpp::45
		char* Symbol = CmdLineProcess->ReadLine(5000);
		char* FileAndLineNumber = CmdLineProcess->ReadLine(5000);

		// find the last colon (before the line number); will always exist
		char* LastColon = strrchr(FileAndLineNumber, ':');
		
		// if this fails, something really weird happened
		if (!LastColon)
		{
			OutLineNumber = 0;
			OutFileName = gcnew String("???");
		}
		else
		{
			// cut off the string at the colon
			*(LastColon) = 0;

			// and pull it apart into two parts
			OutLineNumber = atoi(LastColon + 1);
			OutFileName = gcnew String(FileAndLineNumber);
		}
		
		// use the symbols
		if (Symbol)
		{
			OutFunction = gcnew String(Symbol);
		}
		else
		{
			OutFunction = gcnew String("Unknown");
		}

		return true;
	}

	/**
	 * Creates a formatted line of text for a callstack.
	 *
	 * @param	FileName		The file that the function call was made in.
	 * @param	Function		The name of the function that was called.
	 * @param	LineNumber		The line number the function call occured on.
	 *
	 * @return	A string formatted using "{0} [{1}:{2}]\r\n" as the template.
	 */
	String^ AndroidSymbolParser::FormatCallstackLine(String ^FileName, String ^Function, int LineNumber)
	{
		return String::Format(L"{0} [{1}:{2}]\r\n", Function->EndsWith(L")") ? Function : Function + L"()", FileName->Length > 0 ? FileName : L"???", LineNumber.ToString());
	}
}
