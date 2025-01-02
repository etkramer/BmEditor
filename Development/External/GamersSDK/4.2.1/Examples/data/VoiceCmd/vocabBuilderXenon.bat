echo off
rem Usage: VocabBld <word list.txt> <dictionary.pdc> <nnet.psi> <output.xvocab>
rem   Utility to create a speech recognition file.
rem     <Word list.txt> has one line per word to recognize,
rem                     with each line containing:
rem                     <ID# to return><tab><word or phrase>
rem                     (lines beginning with non-numeric characters are ignored)
rem     <Dictionary.pdc> is a dictionary file provided by Fonix corporation
rem     <AsrNNet.psi>    is a 'packed net file' from Fonix corporation
rem     <Output.xvocab>  is the name of the output file to create.

rem Usage: Buildnnx <OutputFnxVoice.vnn> <OutputFnxVoiceUser.usr> <inputNNet.psi>
rem  <vocab1.xvocab> [vocab2.xvocab...]
rem         [-s MaxSimultaneousVocabs] [-p MaxTotalSearchPaths] [-n MaxTotalNodes]

rem xbmkdir xe:\XVoiceCmd
rem xbmkdir xe:\XVoiceCmd\Data
rem xbmkdir xe:\XVoiceCmd\Media

md Xenon

rem ==== "Tasty5" (5-word, single-user) demo ====
..\..\..\tools\Win32\bin\VocabBld Tasty5.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\USEnglish.psi .\Xenon\Tasty5.xvocab -n 5

..\..\..\tools\Win32\bin\BuildVoice .\Xenon\Tasty5.vnn .\Xenon\Tasty5.usr ..\..\..\Languages\USEnglish\USEnglish.psi .\Xenon\Tasty5.xvocab 

copy .\Tasty5.txt .\Xenon
copy .\Xenon\Tasty5.vnn ..\..\Xenon\XeVoiceCmd\Data
copy .\Xenon\Tasty5.usr ..\..\Xenon\XeVoiceCmd\Data
copy .\Xenon\Tasty5.xvocab ..\..\Xenon\XeVoiceCmd\Data
copy .\Tasty5.txt ..\..\Xenon\XeVoiceCmd\Data

rem ==== "Tasty" (70-word, single-user) demo ====
..\..\..\tools\Win32\bin\VocabBld Tasty.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\USEnglish.psi .\Xenon\Tasty.xvocab -n 10

..\..\..\tools\Win32\bin\BuildVoice .\Xenon\Tasty.vnn .\Xenon\Tasty.usr ..\..\..\Languages\USEnglish\USEnglish.psi .\Xenon\Tasty.xvocab 

copy .\Tasty.txt .\Xenon
copy .\Xenon\Tasty.vnn ..\..\Xenon\XeVoiceCmd\Data
copy .\Xenon\Tasty.usr ..\..\Xenon\XeVoiceCmd\Data
copy .\Xenon\Tasty.xvocab ..\..\Xenon\XeVoiceCmd\Data
copy .\Tasty.txt ..\..\Xenon\XeVoiceCmd\Data

pause
