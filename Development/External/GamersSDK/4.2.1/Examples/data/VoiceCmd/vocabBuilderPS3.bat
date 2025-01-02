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

md PS3

rem ==== "Monsters" (10-word, single-user) demo ====
..\..\..\tools\Win32\bin\VocabBld Monsters.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\USEnglish.psi .\PS3\Monsters.xvocab -n 5

..\..\..\tools\Win32\bin\BuildVoice .\PS3\Monsters.vnn .\PS3\Monsters.usr ..\..\..\Languages\USEnglish\USEnglish.psi .\PS3\Monsters.xvocab 

copy .\Monsters.txt .\PS3

rem ==== "Tasty" (70-word, single-user) demo ====
..\..\..\tools\Win32\bin\VocabBld Tasty.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\USEnglish.psi .\PS3\Tasty.xvocab -n 10

..\..\..\tools\Win32\bin\BuildVoice .\PS3\Tasty.vnn .\PS3\Tasty.usr ..\..\..\Languages\USEnglish\USEnglish.psi .\PS3\Tasty.xvocab 

copy .\Tasty.txt .\PS3

pause
