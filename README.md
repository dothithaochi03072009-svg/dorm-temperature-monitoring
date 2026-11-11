# dorm-temperature-monitoring

## Why this project
I am living in a high school dorm in the US. Our dorm is divided into
suites, and each suite has 3 rooms. Currently, the middle room (room 2)
in the suites is empty, so residents are living in rooms 1 and 3. My
suitemate and I share the same thermostat - each suite shares the same
thermostat - but our rooms often feel different. I wanted
actual numbers. So I built a small sensor system to measure the
temperature in both rooms over time and see how big the difference
really is, and what might be causing it.

## Research questions
1. What external factors affect how warm or cold a room feels (sunlight
   exposure, open windows, number of people in the room, etc.)? How much
   does each factor affect the actual room temperature?
2. How much does the temperature differ between my room and my
   suitemate's room, even though we share the same thermostat? Is that
   difference large enough to explain why my suitemate and I feel the
   temperature differently, or is it just because our bodies feel
   temperature differently from each other?
## What the system is made of
- 2 sensor nodes (ESP32 + NTC thermistor + DS3231 real-time clock),
  one for each room
- 1 receiver (ESP32) that collects data from both nodes
- A Python script on my laptop that logs everything to a CSV file
## How it works
Each node checks the temperature sensor every 10 seconds and reads
the exact time off the DS3231 clock module, then sends that reading
to the receiver over ESP-NOW. If the receiver isn't listening right
then, the node just saves the reading in its own flash memory instead
of losing it. The receiver passes along whatever it gets from both
nodes over USB to my laptop, where a Python script writes it into a
CSV file.
## Timeline
- **Sep 1**: Noticed the temperature difference between the two rooms
  and decided to actually measure it instead of guessing
- **Oct 3**: Wrote the research outline
- **Oct 7**: Started writing the code
- **Oct 10**: Ordered the hardware (ESP32 boards, NTC probes, DS3231
  modules,etc)
- **Oct 26–28**: Wired and tested both sensor nodes and the receiver;
  traced a probe that kept reading "shorted" back to a loose, untrimmed
  wire, and found a naming conflict in the temperature formula
- **Nov 5**: Started the calibration phase: running both sensors side
  by side before placing them in separate rooms
  ## Current status
Both sensor nodes and the receiver are built, coded, and transmitting
data successfully. I'm currently in the calibration phase, running
both sensors side by side in my room and comparing them to a reference
thermometer. So far, N01 matches the reference thermometer, while N02
reads about 1°C lower. I will monitor this for another day or two to
confirm the offset stays consistent before applying a correction and
placing the two nodes in their separate rooms.
## Challenges
Getting the receiver and the two nodes to find each other through 
ESP-NOW took me a few tries because they both have to be set to the
exact same WiFi channel. One of the probes (N02) also kept printing
"LỖI: đầu dò bị chập" (probe error) with the voltage stuck at 3145 mV,
which is way outside the normal range. After checking it, I found
out that the bare wire from the probe was plugged in, but the connection
was still loose. I had to replace the wire and reconnect it before it
worked properly. It was not a wiring or code mistake, just a bad
connection.
Writing this in C++ was its own challenge too, because I'd
only worked in Python and Java before.
