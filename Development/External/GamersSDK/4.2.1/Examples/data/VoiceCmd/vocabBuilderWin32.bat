echo off
rem Usage: VocabBld <word list.txt> <dictionary.pdc> <nnet.pni> <output.xvocab>
rem   Utility to create a speech recognition file.
rem     <Word list.txt> has one line per word to recognize,
rem                     with each line containing:
rem                     <ID# to return><tab><word or phrase>
rem                     (lines beginning with non-numeric characters are ignored)
rem     <Dictionary.pdc> is a dictionary file provided by Fonix corporation
rem     <AsrNNet.pni>    is a 'packed net file' from Fonix corporation
rem     <Output.xvocab>  is the name of the output file to create.

rem Usage: Buildnnx <OutputFnxVoice.vnn> <OutputFnxVoiceUser.usr> <inputNNet.pni>
rem  <vocab1.xvocab> [vocab2.xvocab...]
rem         [-s MaxSimultaneousVocabs] [-p MaxTotalSearchPaths] [-n MaxTotalNodes]

mkdir Win32

rem ==== "Tasty" (70-word, single-user) demo ====
..\..\..\tools\Win32\bin\VocabBld Tasty.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\USEnglish.psi .\Win32\Tasty.xvocab -n 10

..\..\..\tools\Win32\bin\BuildVoice .\Win32\Tasty.vnn .\Win32\Tasty.usr ..\..\..\Languages\USEnglish\USEnglish.psi .\Win32\Tasty.xvocab 

copy tasty.txt .\win32
pause