#! /usr/bin/python
# -*- coding: utf-8 -*-
# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

#   Rentalito
#   Copyright (C) 2013 by Xose Pérez
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

__app__ = "Rentalito"
__version__ = "0.2"
__author__ = "Xose Pérez"
__contact__ = "xose.perez@gmail.com"
__copyright__ = "Copyright (C) 2013 Xose Pérez"
__license__ = 'GPL v3'

import sys
import time
from datetime import datetime
import ctypes

from libs.Daemon import Daemon
from libs.Config import Config
from libs.Mosquitto import Mosquitto
from libs.Processor import Processor

class Rentalito(Daemon):
    """
    Rentalito republish daemon.
    Glues the different components together
    """

    debug = True
    mqtt = None
    processor = None
    publish_to = None
    minimum_time = 5
    time_length_ratio = 0.5
    topics = {}
    slots = []
    slot_index = {}
    next_event_time = time.time()

    def log(self, message):
        """
        Log method.
        TODO: replace with standard python logging facility
        """
        if self.debug:
            timestamp = datetime.now().strftime('%Y-%m-%dT%H:%M:%S.%f')
            sys.stdout.write("[%s] %s\n" % (timestamp, message))
            sys.stdout.flush()

    def cleanup(self):
        """
        Clean up connections and unbind ports
        """
        self.mqtt.disconnect()
        self.log("[INFO] Exiting")
        sys.exit()

    def mqtt_connect(self):
        """
        Initiate connection to MQTT broker and bind callback methods
        """
        self.mqtt.on_connect = self.mqtt_on_connect
        self.mqtt.on_disconnect = self.mqtt_on_disconnect
        self.mqtt.on_message = self.mqtt_on_message
        self.mqtt.on_subscribe = self.mqtt_on_subscribe
        self.mqtt.connect()

    def mqtt_on_connect(self, obj, result_code):
        """
        Callback when connection to the MQTT broker has succedeed or failed
        """
        if result_code == 0:
            self.log("[INFO] Connected to MQTT broker")
            self.mqtt.send_connected()
            for topic, new_topic in self.topics.iteritems():
                self.log("[DEBUG] Subscribing to %s" % topic)
                self.mqtt.subscribe(topic, 0)
        else:
            self.stop()

    def mqtt_on_disconnect(self, obj, result_code):
        """
        Callback when disconnecting from the MQTT broker
        """
        if result_code != 0:
            time.sleep(3)
            self.mqtt_connect()

    def mqtt_on_subscribe(self, obj, mid, qos_list):
        """
        Callback when succeeded subscription
        """
        self.log("[INFO] Subscription with mid %s received." % mid)

    def mqtt_on_message(self, obj, msg):
        """
        Incoming message, enqueue it
        """
        topic = self.topics.get(msg.topic, False)
        if topic:

            try:
                value = ctypes.string_at(msg.payload, msg.payloadlen)
            except:
                value = msg.payload
            index = (self.slot_index.get(msg.topic, 0) + 1) % topic.get('slots', 1)
            self.slot_index[msg.topic] = index

            # Look for the slot
            found = False
            position = 0
            for slot in self.slots:
                if slot['topic'] == msg.topic and slot['index'] == index:
                    found = True
                    break
                position = position + 1
            if not found:
                slot = dict()
            slot['topic'] = msg.topic
            slot['index'] = index
            slot['value'] = value
            slot['repetitions']= topic.get('repetitions', None)
            expires = topic.get('expires', None)
            slot['expires'] = time.time() + expires if expires else None
            prioritary = topic.get('prioritary', 0) == 1
            if found:
                if prioritary:
                    slot = self.slots.pop(position)
                    self.slots.insert(0, slot)
            else:
                if prioritary:
                    self.slots.insert(0, slot)
                else:
                    self.slots.append(slot)

    def push_message(self):
        if self.slots:

            # get next slot
            slot = self.slots.pop(0)

            # if slot has expired return,
            # we are not updating next_event_time so the loop will call
            # this method again very shortly
            if slot['expires'] and slot['expires'] < time.time():
                return

            # preprocess message and send
            value = self.processor.process(slot['topic'], slot['value'])
            self.mqtt.publish(self.publish_to, value)

            # update repetitions and check if we have to reenqueue it
            if not slot['repetitions'] == 0:
                if slot['repetitions']:
                    slot['repetitions'] = slot['repetitions'] - 1
                self.slots.append(slot)

            # calculate next update time
            length = len(value.split('|')[-1:][0])
            time_gap = max(self.minimum_time, length * self.time_length_ratio)
            self.next_event_time = time.time() + time_gap

    def prepare(self):
        """
        Loads predefined messages
        """
        self.slots.append({
            'topic': '/builtin/datetime',
            'index': 1,
            'value': None,
            'repetitions': None,
            'expires': None
        })
        self.processor.add_filter('/builtin/datetime', {
            'type': 'format',
            'parameters': {'format': '{date}|{time}'}
        })

    def run(self):
        """
        Entry point, initiates components and loops forever...
        """
        self.log("[INFO] Starting " + __app__ + " v" + __version__)
        self.mqtt_connect()
        self.prepare()

        while True:
            self.mqtt.loop()
            if time.time() > self.next_event_time:
                self.push_message()

if __name__ == "__main__":

    config = Config('rentalito.yaml')

    manager = Rentalito(config.get('general', 'pidfile', '/tmp/rentalito.pid'))
    manager.stdout = config.get('general', 'stdout', '/dev/null')
    manager.stderr = config.get('general', 'stderr', '/dev/null')
    manager.debug = config.get('general', 'debug', False)
    manager.publish_to = config.get('general', 'publish_to', '/client/rentalito')
    manager.minimum_time = config.get('general', 'minimum_time', 5)
    manager.time_length_ratio = config.get('general', 'time_length_ratio', 0.5)
    manager.topics = config.get('topics', None, {});

    mqtt = Mosquitto(config.get('mqtt', 'client_id'))
    mqtt.host = config.get('mqtt', 'host')
    mqtt.port = config.get('mqtt', 'port')
    mqtt.keepalive = config.get('mqtt', 'keepalive')
    mqtt.clean_session = config.get('mqtt', 'clean_session')
    mqtt.qos = config.get('mqtt', 'qos')
    mqtt.retain = config.get('mqtt', 'retain')
    mqtt.status_topic = config.get('mqtt', 'status_topic')
    mqtt.set_will = config.get('mqtt', 'set_will')
    manager.mqtt = mqtt

    processor = Processor(config.get('filters', None, {}))
    manager.processor = processor

    if len(sys.argv) == 2:
        if 'start' == sys.argv[1]:
            manager.start()
        elif 'stop' == sys.argv[1]:
            manager.stop()
        elif 'restart' == sys.argv[1]:
            manager.restart()
        else:
            print "Unknown command"
            sys.exit(2)
        sys.exit(0)
    else:
        print "usage: %s start|stop|restart" % sys.argv[0]
        sys.exit(2)

