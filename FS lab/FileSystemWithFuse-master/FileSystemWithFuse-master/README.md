
# A Simple Mountable File System

This is part of a project for the Operating Systems class at McGill. Check out the Website: http://arthurkuhn.github.io/FileSystemWithFuse/.

## Goal
The goal of this project is to design and implement a simple file system (SFS) that can be mounted by the user under a directory in the user’s machine. Due to compatibility constraints, this program is built to work under linux only (although it could be adapted to other OSs).

## External Software and Libraries
The Fuse Kernel module is used to modify the operation of the Operating System, allowing us to handle certain system calls in user space instead of kernel space. When I/O system calls are made, they are passed through the Fuse Kernel Module and the Fuse API to be handled by our File System.
The C code for the disk emulator is included.

# Implementation

## Disk Emulator
The disk emulator allows us to create a disk of fixed size. We can also read or write any chosen sector. The emulated disk is stored as a file on the actual file system. The data is therefore persistent across program invocations. We can choose the size and number of sectors to be created, in bytes.

## Data Representation
The goal here is to imitate the way the Linux kernel deals with the file system. We will therefore use the same representation as is used in Linux systems. More info about this [here] (http://www.tldp.org/LDP/tlk/fs/filesystem.html).
 
# Testing

A test file is included
