# 3081 Lab02 C++ The Build Process and Makefiles

## What You Will Learn

1. Basic commands for compiling and linking a C++ program to build the executable.
2. How Makefiles compile and link C++ programs.
3. How to build a Makefile to automate the build process.

### What's in labs/lab02_build_process?

This file:

- README.md

Other files to be used and not modified:

- decoy_duck.h/.cpp
- duck.h/.cpp
- main.cpp
- mallard_duck.h/.cpp
- mute_quack.h/.cpp
- quack_quack.h/.cpp
- quack_behavior.h/.cpp
- rubber_duck.h/.cpp
- squeak_quack.h/.cpp

Review these files, especially if you're looking for more examples of how the "has-a" relationship works, handling pointers and using delete, or just general C++ class syntax.

### Understanding the Build Process

The build process is the combination of compiling and linking to build your executable. Each source file is individually compiled to create an object file. Then all object files are linked together, whereby all symbols are resolved and the executable is created. We will first look at the commands to complete this process, then begin the real work of this lab which is to build a Makefile to automate it.

Navigate to the lab02_build_process directory. Compile a couple of the source files to create object files.

```
$ g++ -c -o decoy_duck.o decoy_duck.cpp
$ g++ -c -o main.o main.cpp
$ g++ -c duck.cpp
```

**What does all that mean?**

- `g++` : call the gnu c++ compiler
- `-c` : a flag to indicate compilation only, as opposed to linking
- `-o` : a flag stating to output to the filename that this precedes
- `main.cpp` : the last filename is the one to be compiled

- If `-c` is not provided, both compilation and linking will occur.
- If `-o` is not provided, the compiler will use the filename from the .cpp file. Notice that duck.o was created in that way.

_A side note: CAREFUL, do not follow the -o with main.cpp because then you will be indicating to output a file named 'main.cpp' and you will overwrite your original main.cpp!_

Notice that each file is independently compiled, even though there are interdependencies among them. If you look in the .cpp file, there will be a #include <filename>.h statement which essentially inserts the text of <filename>.h at the location of the #include statement. Compilation will create symbol tables for all classes, variables, functions declared within those header files, but with no associated address for the definition of those elements. When the files are linked, those dependencies will be resolved and the definitions of the various objects will be included in the complete program.

Just from these few lines of compilation commands, you can see that you really do not want to use the command line for building your executable, especially when there are many files. Before venturing on to the Makefile to automate this, let us look at the command for linking and running our executable.

```
$ g++ -c *.cpp
$ g++ -o duck *.o
$ ./duck
```

This takes all object files in the directory and links them together to make the executable _duck_. If no file were provided, it would name the executable _a.out_. Executables are run with `./<executable_name>`.

> If you try to run the second command without the first, with only the three .o files we manually created earlier, there will be an error about _undefined references_. This makes sense since the .o files for all of the dependencies needed to build the program haven't been generated yet.

### Getting Started with Makefile

A Makefile is a form of script, similar to a shell script, but with some interesting additional features. The most important difference is that Makefiles track file dependencies and will create new object files only for those source files whose dependencies have been modified. As long as you list the dependency, the Makefile will track it. You can imagine that this makes the build process much faster.

Start by creating a Makefile in the current lab directory using your favorite editor (e.g. xemacs, vim, atom, etc.):

```
$ touch Makefile
```

_A side note: In this course, we require you to name your file "Makefile". When you type `make` on the command line, it will first search for the file "Makefile" then "makefile". The `make` command will not recognize any other filename._

### Compile and Link Directly

_A side note: Makefiles can get very sophisticated and complex, but for these labs, we will create very basic ones. Once we get to the project, we will be using the tool "cmake" to make the Makefile._

The standard, basic structure of a makefile is to define variables at the top that set compilation and linking flags, define libraries, and store the filenames that will be used in the build process. This is followed by a collection of commands that specify a target (i.e. the file that should be made), the file dependencies, and the compilation or linking command.

These three elements must appear in the following format:

```
target: dependencies
[tab]command
```

For example, building a compiled object for the MallardDuck class would look like this:

```
mallard_duck.o: mallard_duck.h mallard_duck.cpp
	g++ -c mallard_duck.cpp
```

\***\*Note\*\*** : Some editors replace tabs by spaces when you copy+paste the code from this readme. Ensure that the command is always preceded by a tab and not a series of spaces! For vim users who use spaces instead of tabs, pressing `Ctrl+V` before `Tab` will insert a tab when in insert mode instead of spaces.

Add the above example to your Makefile. First, clear the existing ".o" files, then run `make`.

```
$ rm -f *.o duck
$ make
```

The `make` command searches for the first target listed in the makefile and executes that command. In this case, it finds the `mallard_duck.o` target, which depends on the files mallard_duck.h and mallard_duck.cpp. It will first look to see if mallard_duck.o exists and if not, will execute the command. If the file does exist, then it will check if any of the dependency files listed have changed since the last time it created mallard_duck.o. If there are no changes, then nothing will happen. If there were changes, it will execute the command and echo the command in your terminal.

Now, list the directory contents with 'ls'. You should now see that mallard_duck.o exists on your system. This is the compiled version of your MallardDuck class. It is ready for linking.

Let's try that again to see what happens.

```
$ make
```

Nothing happened! It states that the file is up to date, Which is correct because nothing changed. Go ahead and add a space or change a comment in one of the files, run `make` again and see that it executes the compilation command again to create a new object file.

Let's add another target to our Makefile. Place this AFTER the mallard_duck.o target.

```
rubber_duck.o: duck.h rubber_duck.h rubber_duck.cpp
	g++ -c rubber_duck.cpp
```

This target will build the RubberDuck class. Notice that we've included a secondary dependency, duck.h. If that interface changes, we want to recompile rubber_duck to use the correct version of duck.h and to insure that everything is still compatible.

Let's run make again.

    $ make

**What just happened?** Make responded that mallard_duck.o is up to date. That's true, it is. We didn't make any changes to the dependencies of mallard_duck.o, so there was no need to recompile it. But, what about rubber_duck.o?

The default target for make is the first target listed in the Makefile, and it will only run one target at time. We can get around this by specifying which target to make, like this:

    $ make rubber_duck.o

That's better. Now, the rubber_duck.o object has been created. A little further along, we will show you how to use .PHONY targets to run all the commands.

### Building Projects

We could continue, adding each target/dependency/command into the Makefile one at a time for our ducks. But, that isn't very efficient, especially for large projects. We need a shortcut!

We can create a single command that uses wildcards to match to any target that we provide. Replace the two targets above with this single target.

```
%.o: %.cpp
	g++ -c $^ -o $@
```

**What's happening here?** You can get a sense of what all those symbols are representing by comparing to the above commands.

- `%` : The % symbol is the wildcard. It matches to any <filename>.o target that you want to make, and it uses that matching filename in the dependency list as well.

- `$^` : is a synonym for the slightly longer shortcut $(inputs), which is just the list of all dependencies listed after the target.

- `$@` : is a shortcut meaning the name of the target without the .o extension.

Now, we can use this to build any number of object files. Give it a try.

```
$ make squeak_quack.o
$ make quack_quack.o
$ make mute_quack.o
```

Ultimately, we want to build an executable from all of the compiled objects. You can see that typing `make <target>` is not so great, but thankfully, we can create a single target that will build the executable and make any necessary object files. We can do this by listing object files as dependencies of our target executable.

Place this new target at the **_top_** of your Makefile, so that it is the default target to be built if make is called without arguments. Do not put any returns in the list of dependencies.

```
ducks: duck.o mallard_duck.o decoy_duck.o rubber_duck.o quack_behavior.o mute_quack.o quack_quack.o squeak_quack.o main.o
	g++ $^ -o $@
```

Now, let's run make to build our application.

    $ make

Note it builds any objects that weren't already built, ending with the creation of `ducks`. Now execute it:

    $ ./ducks

### Cleaning your Project Using PHONY Targets

Sometimes dependencies are not listed properly, and even though changes were made to a file, the make command does not generate a new object file or executable. And sometimes the directory is cluttered with .o files that you want to get rid of. You can clean up all these files with one simple make command that uses a phony target. It is phony because it is not a file that we want to create, rather it is a keyword that we use to get make to execute the corresponding command. `clean` is probably the most often used phony target and typically is used to remove all object files and the executable.

Let's add this to the bottom of our Makefile.

```
clean:
	rm *.o ducks

.PHONY: clean
```

**What's .PHONY?** Targets are nearly always tied to actual files on the system. Since there is no 'clean' file, we make sure that make is aware that this is a command we wish to be able to invoke.

You can clean your directory, check that everything is gone, and rebuild.

```
$ make clean
$ ls
$ make
$ ls
```

### Try to Modify and Rebuild

Let's now see how make check on dependencies and only builds what's necessary. Open squeak*quack.cpp and add a third exclamation point (or remove one) in the \_Quack()* method.

> In squeak_quack.cpp:
> cout << "SQUEAK!!!" << endl;

Now, run make again to see what happens.

```
$ make
```

**What just happened?** Assuming all the compiled objects and executable already existed, _make_ should recognize that squeak_quack.cpp has changed. Based on this change, it should rebuild the squeak_quack.cpp compiled object and then rebuild the executable (since it depends on squeak_quack.o).

### Macros

Macros in Makefiles are variables that are used in the target commands. It allows for the specifics of your application to be primarily at the top of the file, and the targets to remain mostly unchanged regardless of the project. For example, in making the executable above, you had to type out a long list of dependencies, but if instead you saved this list in a variable, you could use the same command for every project.

Place these variables at the TOP of your Makefile, above your targets.

```
EXEFILE = ducks
CXXFLAGS = -c -Wall
SOURCES = duck.cpp mallard_duck.cpp decoy_duck.cpp rubber_duck.cpp quack_behavior.cpp mute_quack.cpp quack_quack.cpp squeak_quack.cpp main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
```

And if you really do not want to type all those filenames and you know that every source file in your directory needs to be compiled, use this instead:

```
SOURCES = $(wildcard *.cpp)
```

**What are these?** We are giving a name to the executable to be created and defining the g++ compiler flags we will use in our compile (but not linking) commands.

We need to make a few changes in our Makefile to use these Macros. First, in the _ducks_ target, replace the target name with our macro:

```
$(EXEFILE): $(OBJECTS)
	g++ $^ -o $@
```

In the %.o target, replace the '-c' flag in the current command with '$(CXXFLAGS)'. Notice the `-Wall` that was added. This means show all warnings, which you should always use and pay attention to. The compiler is usually trying to tell you something important that is likely to cause problems during execution!

```
%.o: %.cpp
	g++ $(CXXFLAGS) $^ -o $@
```

And, finally, update our clean target:

```
clean:
	rm *.o $(EXEFILE)
```

Now, let's make sure our Macros are being used. Check the output to be sure that the new compiler flags are being utilized.

```
$ make clean
$ ls
$ make
$ ls
```

This will create an executable 'ducks'. Finally, check if the application runs without errors.

      $ ./ducks

> For those wanting to play around with terminal commands, a quick way to get the same file you created before is to _copy_ it: `$cp <path to lab01 folder> <path to lab02 folder>`. The general form of a _copy_ command is `$cp <path to src> <path to dest>`.

THIS LAB IS COMPLETE.

Congratulations!
