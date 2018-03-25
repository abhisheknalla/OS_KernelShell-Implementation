--------------------------------------------------------

TITLE :- Create an interactive user defined shell

--------------------------------------------------------

-------------------------------------------------------

Conditions :- system(),popen, pclose call should not be used

-------------------------------------------------------

-------------------------------------------------------

Procedure for running shell :-

-------------------------------------------------------
     step1 :- Run "make" command in this directory where Makefile is present in the shell.
     step2 :- Run "./bash" command on shell.

This will start the user defined shell.


-------------------------------------------------------

Description :-

Shell script consists of 3 files :- 1.)Bash.c 2.) n_ls.c 3.) pinfo.c		
					


Bash.c :- This consists of main code of the shell.This includes methods      for commands like pwd,echo,cd. Also main method for creating    processes lies with in it.

----------------------------------------------------------



----------------------------------------------------------

USAGE :- pwd -> prints present working directory.

	 cd -> changes directory to the one specified in the input.
         
	 echo -> prints the input on the terminal.
         
	 n_ls(-l -a,-l ,-a ,-la ,-al) -> same as ls displays 
         information of the directories and files.
         
	 & -> if & is kept at end of any command,shell executes that 
         process in the background.
         
	 pinfo(PID) -> it returns information of current running processes 
         or information about process with given PID.
         
	 Redirection(>) -> ex:- ls > a.txt
         
	 Pipes -> more a.txt | wc
         
	 setenv:- setenv var=value
         
	 unsetenv:- unsetenv var
         
	 jobs :- Prints the jobs
         
	 kjob:- kjob 2 9
         
	 fg :- Brings to fg
         
	 bg :- Resumes process in background
         
	 overkill :- Kills all background processes
         
	 quit :- exits from user-defined shell
         
	 ctrl-Z :- change status of the te process
         
	 ctrl-c :- Give SIGINT signal

 You can run any command using this user defined shell.

------------------------------------------------------
