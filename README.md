Building (linux.student.cs.uwaterloo.ca)
========================================

Clone repo and compile commands within terminal:
---------------------------------------------------------
git clone https://git.uwaterloo.ca/dduranro/kernel.git
cd kernel
git checkout 763b721388474c939668170d8d3150bd5f1acacd

Now, if uploading to "track a" do:
make a

And for "track b" do:
make b
---------------------------------------------------------

This will produce tc1.img which should be deployed to the RPi and run. 
See the course web page for deployment instructions.

Upon running, the UI will initialize by setting all train speeds to zero and all turnouts to curved.
The user will be unable to input keyboard commands during this time.
Note that the program assumes that only trains 14 or 18 will be used.
Please see the submitted PDF on LEARN for further program details

Some basic commands:
tr <train number> <train speed> – Set any train in motion at the desired speed (0 for stop, 14 for full speed)
stop <train number> <target> <offset> – Stops a train at the desired location
rv <train number> – The train will reverse direction.
sw <switch number> <switch direction> – Throw the given switch to straight (S) or curved (C)
q – halt the program and return to boot loader (reboot). 

Every command must have the ENTER key sent after typing.
