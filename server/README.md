# Rentalito Server

This daemon will enqueue and deliver messages from different topics to a given one.

## Requirements

* python-yaml
<pre>sudo apt-get install python-yaml</pre>

* python-mosquitto
<pre>sudo apt-get install python-mosquitto</pre>

## Install

Just clone or extract the code in some folder. I'm not providing an setup.py file yet.

## Configuration

Rename or copy the rentalito.yaml.sample to rentalito.yaml and edit it. The configuration is pretty straight forward:

### general

### mqtt

These are standard Mosquitto parameters. The status topic is the topic to post messages when the daemon starts or stops.

### topics

### filters

## Running it

The util stays resident as a daemon. You can start it, stop it or restart it (to reload the configuration) by using:

<pre>python rentalito.py start|stop|restart</pre>



