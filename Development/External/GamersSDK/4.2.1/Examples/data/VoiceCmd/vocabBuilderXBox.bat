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

rem xbmkdir xe:\XVoiceCmd
rem xbmkdir xe:\XVoiceCmd\Data

md XBox

rem ==== "Tasty5" (5-word, single-user) demo ====
..\..\..\tools\Win32\bin\VocabBld Tasty5.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\USEnglish.pni .\XBox\Tasty5.xvocab .\XBox\PhoneData.vpi -n 5

..\..\..\tools\Win32\bin\BuildVoice .\XBox\Tasty5.vnn .\XBox\Tasty5.usr ..\..\..\Languages\USEnglish\USEnglish.pni .\XBox\Tasty5.xvocab 

rem xbcp Tasty5.vnn Tasty5.usr Tasty5.xvocab Tasty5.txt xe:\XVoiceCmd\Data\

rem ==== "Tasty" (70-word, single-user) demo ====
..\..\..\tools\Win32\bin\VocabBld Tasty.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\USEnglish.pni .\XBox\Tasty.xvocab -n 10

..\..\..\tools\Win32\bin\BuildVoice .\XBox\Tasty.vnn .\XBox\Tasty.usr ..\..\..\Languages\USEnglish\USEnglish.pni .\XBox\Tasty.xvocab 

rem xbcp Tasty.vnn Tasty.usr Tasty.xvocab Tasty.txt xe:\XVoiceCmd\Data\

rem ==== "Monsters" (multi-user) demo ====

..\..\..\tools\Win32\bin\VocabBld Monsters.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\USEnglish.pni .\XBox\Monsters.xvocab -n 5

..\..\..\tools\Win32\bin\BuildVoice .\XBox\Monsters.vnn .\XBox\Monsters.usr ..\..\..\Languages\USEnglish\USEnglish.pni .\XBox\Monsters.xvocab 

rem xbcp Monsters.vnn Monsters.usr Monsters.xvocab Monsters.txt xe:\XVoiceCmd\Data\


rem ==== Japanese demo ====
..\..\..\tools\Win32\bin\VocabBld Japanese.txt ..\..\..\Languages\Japanese\Japanese.pdc ..\..\..\Languages\Japanese\Japanese.pni .\XBox\Japanese.xvocab -n 5

..\..\..\tools\Win32\bin\BuildVoice .\XBox\Japanese.vnn .\XBox\Japanese.usr ..\..\..\Languages\Japanese\Japanese.pni .\XBox\Japanese.xvocab 

rem xbcp Japanese.vnn Japanese.usr Japanese.xvocab Japanese.txt xe:\XVoiceCmd\Data\


