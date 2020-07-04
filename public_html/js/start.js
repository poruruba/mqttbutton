'use strict';

//var vConsole = new VConsole();

const mqtt_url = "【MQTTブローカのURL(WebSocket接続)】";
var button_setting = [
    {
        enable: true,
        char: "a",
        fore: "#ffffff",
        back: "#000000",
        rotate: 0
    },
    {
        enable: true,
        char: "b",
        fore: "#ff0000",
        back: "#000000",
        rotate: 0
    },
    {
        enable: true,
        char: "c",
        fore: "#00ff00",
        back: "#000000",
        rotate: 0
    },
    {
        enable: true,
        char: "d",
        fore: "#0000ff",
        back: "#000000",
        rotate: 0
    },
    {
        enable: true,
        char: "e",
        fore: "#ffff00",
        back: "#000000",
        rotate: 0
    },
];

var mqtt_client = null;

var vue_options = {
    el: "#top",
    data: {
        progress_title: '', // for progress-dialog

        mqtt_url: mqtt_url,
        connected: false,
        received_mode: null,
        received_id: null,
        received_index: null,
        id: 0,
        mode: 1,
        brightness: 20,
        received_date: null,
        btns: button_setting
    },
    computed: {
    },
    methods: {
        mqtt_onMessagearrived: async function(message){
            var topic = message.destinationName;
            console.log(topic);
            switch(topic){
                case "btn/m5atom": {
                    var body = JSON.parse(message.payloadString);
                    console.log(body);
                    this.received_date = new Date().toLocaleString();
                    this.received_mode = body.mode;
                    this.received_id = body.id;
                    this.received_index = body.index;
                    break;
                }
                default:
                    console.log('Unknown topic');
                    break;
            }
        },
        mqtt_onConnectionLost: function(errorCode, errorMessage){
            console.log("MQTT.onConnectionLost", errorCode, errorMessage);
            this.connected = false;
        },
        mqtt_onConnect: async function(){
            console.log("MQTT.onConnect");
            this.connected = true;
            mqtt_client.subscribe("btn/m5atom");
        },
        connect_mqtt: async function(){
            mqtt_client = new Paho.MQTT.Client(this.mqtt_url, "browser");
            mqtt_client.onMessageArrived = this.mqtt_onMessagearrived;
            mqtt_client.onConnectionLost = this.mqtt_onConnectionLost;
            
            mqtt_client.connect({
                onSuccess: this.mqtt_onConnect
            });
        },
        publish_setting: function(){
            if( !this.connected ){
                alert("MQTTブローカに接続していません。");
                return;
            }

            var data = {
                mode: this.mode,
                id: this.id,
                brightness: this.brightness,
                btns: []
            };
            for( var i = 0 ; i < this.btns.length ; i++ ){
                if( !this.btns[i].enable )
                    continue;
                var btn = {
                    char: this.btns[i].char,
                    fore: this.rgb2num(this.btns[i].fore),
                    back: this.rgb2num(this.btns[i].back),
                    rotate: this.btns[i].rotate
                };
                data.btns.push(btn);
            }
            var message = new Paho.MQTT.Message(JSON.stringify(data));
            message.destinationName = "btn/m5atom_setting";
            mqtt_client.send(message);
        },
        rgb2num: function(rgb){
            return parseInt(rgb.slice(1, 7), 16);
        },
    },
    created: function(){
    },
    mounted: function(){
        proc_load();
    }
};
vue_add_methods(vue_options, methods_bootstrap);
vue_add_components(vue_options, components_bootstrap);
var vue = new Vue( vue_options );
