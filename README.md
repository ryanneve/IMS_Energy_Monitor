# README #

### What is this repository for? ###

* Teensy based power monitor developed for use on Autonomous Vertical Profilers.
* Uses ADS1115 16 bit ADC to monitor Vcc, battery voltage, load current and charge current.
* Current measured with [ACS715](https://www.pololu.com/file/download/ACS715.pdf?file_id=0J197)
* Calculates power and energy
* Uses [teensy's RTC](https://www.pjrc.com/teensy/td_libs_Time.html)

### How do I get set up? ###

* Runs on [Teensy 3.1/3.2](https://www.pjrc.com/store/teensy32.html). Communicates via USB.
* [Communications Specification](https://sites.google.com/site/verticalprofilerupgrade/home/ControllerSoftware/ipc-specification)
* uses aJson library ([original](https://github.com/interactive-matter/aJson)) or ([my fork])(https://github.com/ryanneve/aJson))

### Who do I talk to? ###

Ryan Neve
neve@email.unc.edu