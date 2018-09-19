# Mancala Sockets 
#####


Multiplayer version of one of the [oldest board games](https://en.wikipedia.org/wiki/Mancala) in the world!

Instructions are simple and will be sent when the game starts or see the [instructions here](https://en.wikipedia.org/wiki/Mancala#General_gameplay)

## Features

#####
* Play from right inside the shell
* Unlimited players
* Protection from player disconnects
* Server has simple admin commands (kick, ban, broadcast, etc)


## Technologies Used
* C (Socket Programming)
* Bash (`nc`)
   
## Deployment
#####
### Server 
#####
Unfortunately, this project is not cross platform so you will have to compile the server on your machine. 

Clone the repository on to your local machine and compile `mancsrv.c` with `gcc -WALL std=c99 -g -o mancsrv.o mancsrv.c`. After, simply run with `./mancsrv.o -p <PORT>` (`<PORT>` being the port you desire (defaults to 3000)), and you're done. You will be prompted if players join.

### Client

Use `nc <SERVER-IP> <PORT>` (`<SERVER-IP>` being your server IP or `localhost` if on the same machine and `<PORT>` being the port you specified eariler) to connect, then simply follow the instruction prompts!

Have fun!
