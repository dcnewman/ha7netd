## ha7netd

ha7netd is a daemon process which generates local weather data from
raw data collected from [Maxim 1-Wire](http://www.maximintegrated.com/en/products/comms/one-wire.html) devices such as temperature, pressure, and
humidity sensors.  All first and second-order temperature corrections
are applied as per the relevant Maxim data sheets for the devices
in question.  More advanced weather data is computed as per meteorological
standards used in the United States (e.g., station to sea-level
pressure correction takes into account diurnal effects by using
data from 12 hours previous).

ha7netd communicates with 1-Wire devices using a HA7net host adapter.
The HA7net is an ethernet to 1-Wire adapter manufactured and sold
by [Embedded Data Systems](http://www.embeddeddatasystems.com/).
As such, ha7netd acts as an HTTP client and talks over TCP/IP to
a HA7net.

ha7netd performs the following tasks

1. On startup, connects to a HA7net device and enumerates all the
   1-Wire devices connected to the HA7net.  Based upon the serial
   numbers of the devices, ha7netd can auto-configure the types of
   information it will gather.  Through the ha7netd configuration
   file, logical groupings can be made of more complicated sensors
   which incorporate multiple 1-Wire devices (e.g., a humidity
   sensor which includes both a temperature sensor for compensation
   calculations and an actual humidity sensor).

2. Periodically, the 1-Wire devices are sampled, the raw data collected
   and derived values computed.

3. Raw data and derived data is output into a text file which is rolled
   over on a daily basis.

4. Raw, derived data, and historical data is output in an XML file which
   then can be post processed and converted to HMTL or other formats
   using XSLT or other tools.  ha7netd can launch the post processing
   command.
