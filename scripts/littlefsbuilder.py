
Import("env")
from sys import platform

print("Replace MKSPIFFSTOOL with mklittlefs")
if platform == "linux" or platform == "linux2":
    env.Replace (MKSPIFFSTOOL = "tools/mklittlefs.x86_64-linux-gnu")
elif platform == "darwin":
    # OS X
    env.Replace (MKSPIFFSTOOL = "tools/mklittlefs.darwin")
elif platform == "win32":
    env.Replace (MKSPIFFSTOOL = "C:/Users/vince/Documents/GitHub/mklittlefs/mklittlefs.exe")