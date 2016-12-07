**Regmon - A LuCi Application for statistical analysis of RegMon counters**

### How to install Regmons LuCI web application###

Checkout Lede sources:

`git clone https://git.lede-project.org/source.git`

Install the RegMon patches \(see [RegMon/README.md](https://github.com/thuehn/RegMon)\)

Enable LuCI feed in your feeds.conf.

Add regmon feed to your feeds.conf:

`src-git regmon https://github.com/thuehn/RegMon`

Update your feeds:

`./scripts/feeds update`

Install luci, luafilesystem and regmon packages:

`./scripts/feeds install -a -p luci`

`./scripts/feeds install -p -a luafilesystem`

`./scripts/feeds install -p -a luci-app-regmon`

Select the following packages in your menuconfig:

- luci -> collections -> luci
- luci -> applications -> luci-app-regmon

Finally build and flash Lede.
In LuCI a new sub menu entry "Regmon" is available under menu entry "Statistics".

###How to configure LuCI web application###

Timespans:

All graphs are displayed for a specified timespan. You can add or remove the available timespans in the "RRDTool" section of statistics output plugin setup. Select "Statistics -> Setup -> Output plugins -> RRDTool". At this page you can modify the whitespace seperated list of timespans in the field "Stored timespans", i.e. adding "1min 5min 10min 20min 30min" to the default list "1hour 1day 1week 1month 1year".

All further configuration options reside at regmons configuration page under "Statistics -> Regmon -> Config"

####Regmon entries####

All entries for Regmon depends on Regmons debugfs settings heayily. With the default configuration the Regmon will output the following comma seperated registers:

- kernel timestamp in milli seconds as a decimal number
- remaining kernel timestamp in nano seconds from last milli second as a decimal number
- TSF timestamp with leading "0x" as a hexadecimal number
- absolute mac bytes counter with leading "0x" as a hexadecimal number
- absolute transmitted bytes counter with leading "0x" as a hexadecimal number
- absolute received bytes counter with leading "0x" as a hexadecimal number
- absolute busy counter with leading "0x" as a hexadecimal number
- lower TSF timestamp with leading "0x" as a hexadecimal number

Regmon path:

The regmon path list option holds one or multiple paths to regmons debugfs directory. Default regmon path is "/sys/kernel/debug/ieee80211/phy0/regmon". You can add another path to another physical device if you have more than one.

Sampling rate:

The sampling rate options specifies regmons sampling rate. This defines the amount of the collection time in nano seconds of Regmon in that Regmon will write one line into the "register_log" file. Good values are a half or one second here (in nano seconds 500000000 or 1000000000). The maximum value is one second.

Time from field:

This options defines the zero based index number of the field from that collectd will take the timestamp from. This option will be ignored when the next option "Use system time" was set.

Use system time:

With this option you can specify whether the collectd should use the system time or not. When this option was not set then the option "Time from field" have to be set to the zero based index of the timestamp field in the "register_log" file.

Absolute counter from field:

This option denotes the zero based index of the absolute mac counter field in the "register_log" file used as the maximum occured ticks on that mac in the last intervall.

Start index of relative counter fields:

With this option you can assign the zero based index of the first field in the "register_log" file of all fields that will be handled relativly to the mac counter field in the statistics.

Register Log Fields:

With this option you can modify the list of all names of the counter fields including the absolute mac counter field. These names appears in the statistics.

####Collectd entries####

In this section you can set the collection interval of collectd in seconds and flag whether the collectd should create a log file for debugging. This option needs the package "collectd-mod-logfile" installed.

####RRDtool entries####

In this section you can modify the parameters of the displayed graph and other RRDTool settings.

RRDTool image path:

The image path option sets the path to a directory for the created graph images. The default is "/tmp/regmon".

Image width and height:

With the next two options you can customize the width and the height of the created graph in pixels. Default is 800x400 pixels.

Stacked graph:

This option enabled stacking on the graph. When enabled all field values are displayed ontop of the previous field values.

Graph shape:

The graph shape option let you choose whether the values are displayed as lines or as filled areas.

Highlight lines:

In area shaped graphs this option let RRDTool draw a highlight line between the areas.

###Graph###

The graph page is displaying the graph and sets the displayed timespan. Please click "refresh" after selecting another timespan. The page has the autorefresh feature enabled. You can disable it at the top right corner of the page.
