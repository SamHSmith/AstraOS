### AstraOS's development is hosted on [source hut](https://sr.ht/~samhsmith/AstraOS/). Talk to us and report bugs there.
### Warning: this project doesn't actually have a readme yet. I don't believe in creating such things before there is more substance to the project.

Source: http://samhsmith.net/blog/astraos_v0_2_is_here.gmi
----------------------------------------------------------
# AstraOS v0.2 is finally here!

What things are new from version 0.1? Well, in no particular order,
* Streams aka Fifo's
* A terminal called dave_terminal
* Multicore support
* User Threading api
* Thread Groups
* Inter Process Function Calls aka IPFC's
* Two gui-app api's built with ipfc's
Going along with the new features are of course fixes to the many bugs I found in previous versions. Playing around with this version is a whole lot more fun then v0.1, it's almost exponential the effect of adding more to an Operating System. We still have a very long way to go though. The next obvious thing for me to work on is the file api. Right now you can only launch new programs from files and navigate a directory structure. Once you can read and write files, the core pillers of the OS will exist. Graphics, User Input, Inter Process Communication, Persistent Data Storage. That's something you can start building on top of.

Tutorial videos and articles concerning all features will be made over the coming weeks. If you have any questions or would like some help concerning AstraOS, do email me at sam.henning.smith@protonmail.com.
----------------------------------------------------------


If you want to build and run AstraOS the best resource is this blog post I made: [gemini://samhsmith.net/blog/astraos_tutorial_from_nothing_to_window_with_colours.gmi](gemini://samhsmith.net/blog/astraos_tutorial_from_nothing_to_window_with_colours.gmi)

If you don't have a gemini client you can view it through this proxy, but be aware it will be a worse experience: [https://proxy.vulpes.one/gemini/samhsmith.net/blog/astraos_tutorial_from_nothing_to_window_with_colours.gmi](https://proxy.vulpes.one/gemini/samhsmith.net/blog/astraos_tutorial_from_nothing_to_window_with_colours.gmi)

If you don't run an arch based distro maybe you can figure out the correct packages for your distro and create a set of instructions for those who follow in your footsteps?
I'll put it here for all to see.

If you want to chat over matrix we have an AstraOS matrix room. #astraos:matrix.org
