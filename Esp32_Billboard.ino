// give me credits for use 
// created by krishna upx61
// thanks for support 
// if you not give me cedits i will stop to make this awesome type projects

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPAsyncWiFiManager.h> // Captive portal

#define SD_CS 5

const byte DNS_PORT = 53;
const char* ssid = "The Shopping Spot";
const char* password = "";

DNSServer dnsServer;
WebServer server(80);

struct Item {
  String name;
  String price;
  String imgPath;
};

Item items[4];
const char* itemDataFile = "/items.json";

// ---------------- HTML embedded ----------------

const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Shopping</title>
<style>
body { font-family: Arial; background:#f4f4f4; text-align:center;}
h1 { color:#333; }
.item { display:inline-block; margin:10px; padding:10px; border:1px solid #ccc; border-radius:10px; background:#fff;}
.item img { width:150px; height:150px; object-fit:cover;}
</style>
</head>
<body>
<h1>Shopping Site</h1>
<div id="items"></div>
<script>
fetch('/items.json').then(r=>r.json()).then(data=>{
  let html='';
  data.items.forEach(it=>{
    html+=`<div class="item">
      <img src="${it.imgPath}" alt="${it.name}">
      <h3>${it.name}</h3>
      <p>Price: ₹${it.price}</p>
      <input type="text" value="${it.imgPath}" readonly>
    </div>`;
  });
  document.getElementById('items').innerHTML=html;
});
</script>
</body>
</html>
)rawliteral";

const char* admin_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Admin Panel</title>
<style>
body { font-family: Arial; background:#f4f4f4; text-align:center;}
h1 { color:#333; }
.item { margin:10px; padding:10px; border:1px solid #ccc; border-radius:10px; background:#fff;}
img { width:100px; height:100px; object-fit:cover;}
</style>
</head>
<body>
<h1>Admin Panel</h1>
<h2>Upload Image</h2>
<form id="uploadForm">
<input type="file" id="file" name="file">
<button type="submit">Upload</button>
</form>
<h2>Items</h2>
<div id="items"></div>
<footer style="margin-top:20px;color:#555;font-size:12px;">
Created by Krishna UPX61 - Thanks for support
</footer>
<script>
function loadItems(){
  fetch('/items.json').then(r=>r.json()).then(data=>{
    let html='';
    data.items.forEach((it,i)=>{
      html+=`<div class="item">
      <img src="${it.imgPath}" alt="${it.name}">
      <p>Name: <input id="name${i}" value="${it.name}"></p>
      <p>Price: <input id="price${i}" value="${it.price}"></p>
      <p>Image: <input id="img${i}" value="${it.imgPath}"></p>
      <button onclick="update(${i})">Update</button>
      </div>`;
    });
    document.getElementById('items').innerHTML=html;
  });
}
function update(i){
  let name=document.getElementById('name'+i).value;
  let price=document.getElementById('price'+i).value;
  let img=document.getElementById('img'+i).value;
  fetch(`/update?index=${i}&name=${encodeURIComponent(name)}&price=${encodeURIComponent(price)}&img=${encodeURIComponent(img)}`).then(loadItems);
}
document.getElementById('uploadForm').onsubmit=function(e){
  e.preventDefault();
  let file=document.getElementById('file').files[0];
  let form=new FormData();
  form.append('file',file);
  fetch('/upload',{method:'POST', body:form}).then(r=>r.text()).then(alert).then(loadItems);
}
loadItems();
</script>
</body>
</html>
)rawliteral";

// ---------------- Setup ----------------

void setup() {
  Serial.begin(115200);

  // SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS mount failed!");
  }

  // SD card
  if(!SD.begin(SD_CS)){
    Serial.println("SD init failed!");
  }

  // Load items
  loadItems();

  // Captive portal
 WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Max power
  // Start Access Point
   WiFi.softAP(ssid);
  Serial.println("AP IP: " + WiFi.softAPIP().toString());

  dnsServer.start(53, "*", WiFi.softAPIP());
  // Web routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200,"text/html",index_html);
  });

  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(adminUser,adminPass)) return request->requestAuthentication();
    request->send(200,"text/html",admin_html);
  });

  // Serve images
  server.serveStatic("/images", SPIFFS, "/images");
  server.serveStatic("/sd", SD, "/");

  // JSON
  server.on("/items.json", HTTP_GET, [](AsyncWebServerRequest *request){
    String json; serializeItems(json);
    request->send(200,"application/json",json);
  });

  // Update item
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(adminUser,adminPass)) return request->requestAuthentication();
    int i=request->getParam("index")->value().toInt();
    items[i].name=request->getParam("name")->value();
    items[i].price=request->getParam("price")->value();
    items[i].imgPath=request->getParam("img")->value();
    saveItems();
    request->send(200,"text/plain","Updated");
  });

  // Upload
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){},
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if(!final) return;
      String path = "/images/" + filename;
      File f = SPIFFS.open(path,"w");
      if(f){
        f.write(data,len);
        f.close();
      }
    }
  );

  server.on("/generate_204", []() { server.sendHeader("Location","/"); server.send(302); });
server.on("/fwlink", []() { server.sendHeader("Location","/"); server.send(302); });
server.on("/hotspot-detect.html", []() { server.sendHeader("Location","/"); server.send(302); });
server.on("/connecttest.txt", []() { server.sendHeader("Location","/"); server.send(302); });

  server.begin();
  Serial.println("Server started. Connect to http://192.168.4.1");
  
}

void loop(){}

// ---------------- Helper ----------------

bool isAuthenticated(AsyncWebServerRequest *request){
  if(!request->authenticate(adminUser,adminPass)) return false;
  return true;
}

void loadItems(){
  if(SPIFFS.exists(itemDataFile)){
    File f = SPIFFS.open(itemDataFile,"r");
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc,f);
    if(!err){
      for(int i=0;i<4;i++){
        items[i].name = doc["items"][i]["name"].as<String>();
        items[i].price = doc["items"][i]["price"].as<String>();
        items[i].imgPath = doc["items"][i]["imgPath"].as<String>();
      }
    }
    f.close();
  } else {
    items[0] = {"Coffee","50","/images/coffee.jpg"};
    items[1] = {"Chai","30","/images/chai.jpg"};
    items[2] = {"Item3","0","/images/item3.jpg"};
    items[3] = {"Item4","0","/images/item4.jpg"};
    saveItems();
  }
}

void saveItems(){
  DynamicJsonDocument doc(1024);
  for(int i=0;i<4;i++){
    doc["items"][i]["name"]=items[i].name;
    doc["items"][i]["price"]=items[i].price;
    doc["items"][i]["imgPath"]=items[i].imgPath;
  }
  File f = SPIFFS.open(itemDataFile,"w");
  serializeJson(doc,f);
  f.close();
}

void serializeItems(String &out){
  DynamicJsonDocument doc(1024);
  for(int i=0;i<4;i++){
    doc["items"][i]["name"]=items[i].name;
    doc["items"][i]["price"]=items[i].price;
    doc["items"][i]["imgPath"]=items[i].imgPath;
  }
  serializeJson(doc,out);
}
// created by krishna upx61
