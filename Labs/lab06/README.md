# Lab 06: Front End Prototype

## Goal

In this lab you will build, install, and run a front end prototype of a GIS system (GopherGIS).  The Goal is to learn how to use `CMake`, a cross platform compilation tool, to build the application on your environment and operating system.

## What You Will Learn

1. How to build and run a computer graphics program.
2. How to use CMake to compile C/C++ code across different platforms.
3. How to obtain API keys and download test data for a GIS application.
4. The skills needed to work on the C++ project.

## What's in lab05

- `lib`: Several dependencies needed for the computer graphics code.

# Overview

## 1. General

The MapViewer application, a prototype for the GopherGIS framework, shows satellite imagery as a 2D map and as a 3D view. The 3D view
uses three data files:

- The terrain file gives the height of the ground.
- The buildings file gives the shape and the height of each building.
- The roads file gives the roads. The route graph comes from this file.

The files are in the `data` folder. Each file is the response
of a web service. The viewer does not change the files. The name of each file
shows the area that the file covers.

The functions in `Fetchers.cpp` will download these files. For tests,
use the script `fetch_prototype_data.sh` to download the files.

## 2. Build and run

Run all the commands from the root folder of the repository. The app must
start from this folder, because it finds the `data` folder from there.

### 2.1 Linux

You will need to have the following packages installed:
- `cmake`
- `build-essential` (for Ubuntu/Debian-like distros) / `c-development` (for Fedora-like distros. Must be installed via `dnf group install`)
- `libglu1-mesa-dev` (for Ubuntu/Debian-like distros) / `mesa-libGLU-devel` (for Fedora-like distros)
- `xorg-dev` (for Ubuntu/Debian-like distros) / `libXi-devel libXcursor-devel libXinerama-devel libXrandr-devel libXxf86vm-devel xorg-x11-server-devel` (for Fedora-like distros)
- `libcurl4-openssl-dev` (for Ubuntu/Debian-like distros) / `libcurl-devel` (for Fedora-like distros)

Build:

```bash
mkdir build
cd build
cmake ..
make
```

Run:

```bash
cd ..
./build/MapViewer
```

### 2.2 macOS (Apple Silicon)

If you don't have [Homebrew](https://brew.sh/) (`brew`) installed already, you will need to install it in order to install all of the libraries needed for the project. Homebrew is a package manager that can run on macOS, making installation of libraries easier.

1. Install the command line tools and CMake:
   ```bash
   xcode-select --install
   brew install cmake
   ```
2. Install the libraries:
   ```bash
   brew install pkg-config
   brew install glfw3
   brew install glm
   ```
3. Build and run the app. Use the commands in 2.1.

NOTE: Apple supports OpenGL up to version 4.1 (core profile). On macOS, the
app asks for OpenGL 4.1. On Linux and Windows, it asks for OpenGL 4.3. You do
not have to change the code.

### 2.3 Windows

Use WSL or a virtual machine. Here is a link to instructions detailing how to install Ubutnu on WSL: [https://canvas.umn.edu/courses/579297/discussion_topics/3657981](https://canvas.umn.edu/courses/579297/discussion_topics/3657981).

Follow the instructions for Linux (2.1) following installation.

## 3. Data files and area

| Data          | File                                           | Web service                  |
|---------------|------------------------------------------------|------------------------------|
| Terrain       | `data/terrain/dem_USGS10m_<S>_<W>_<N>_<E>.asc` | OpenTopography               |
| Buildings     | `data/osm/buildings_<S>_<W>_<N>_<E>.json`      | Overpass API (OpenStreetMap) |
| Roads         | `data/osm/roads_<S>_<W>_<N>_<E>.json`          | Overpass API (OpenStreetMap) |
| Imagery tiles | `data/imagery/<z>-<y>-<x>.jpg`                 | ArcGIS World Imagery         |

`<S>`, `<W>`, `<N>` and `<E>` are the south, west, north and east limits of
the area, in degrees.

The area for the prototype is:

| South  | West    | North  | East    |
|--------|---------|--------|---------|
| 44.939 | -93.288 | 45.009 | -93.168 |

This area is 9.4 km from west to east and 7.8 km from south to north. Its
center is the start location of the app (44.974, -93.228, UMN campus). At zoom
15, the 3D scene is 4.3 km by 3.5 km. The area is larger than the scene by
approximately 2 km on each side.

NOTE: When you click **Render**, the app uses `WebService.cpp` to download the
imagery tiles. You do not have to download them.

## 4. Download the data for tests

### 4.1 Get an OpenTopography API key

You must have an API key to download the terrain. Do this procedure one time
only.

1. Go to opentopography.org.
2. Make an account.
3. Open **MyOpenTopo**.
4. Click **Request an API key**.
5. Copy the key.

CAUTION: Do not put the API key in git. Other persons can see the key and use
it.

### 4.2 Run the script

1. Open a terminal. Go to the root folder of the repository:
   ```bash
   cd lab06
   ```
2. Set the API key:
   ```bash
   export OPENTOPO_API_KEY=paste-your-key-here
   ```
3. Run the script:
   ```bash
   chmod +x fetch_prototype_data.sh
   ./fetch_prototype_data.sh
   ```
4. Make sure that the last line of the output is as follows:
   ```bash
   terrain:   OK (762 lines)
   buildings: OK (50466 elements)
   roads:     OK (10127 elements)
   All data is in place.
   ```

The script downloads only the files that are not in the data folder. To
download a file again, delete the file. Then run the script again.

The download of the buildings can take a few minutes. At the end, the script
examines all the files.

| Option      | Function                                                  |
|-------------|-----------------------------------------------------------|
| `--imagery` | The script also downloads the 20 imagery tiles of the start view. |
| `--check`   | The script only examines the files. It does not download. |

The script sends the buildings request and the roads request to three
Overpass servers, one after the other. If a server is busy, the script uses
the next server. To use only one server, set `OVERPASS_URL`:

```bash
OVERPASS_URL=https://maps.mail.ru/osm/tools/overpass/api/interpreter ./fetch_prototype_data.sh
```
**Steps 5-7 details how the script works under the hood, you do not need to do the following steps.**

## 5. Web service requests

This section gives the requests that the script sends. The fetchers must send
the same requests.

### 5.1 Terrain

The terrain comes from the USGS 3DEP data set `USGS10m`. The distance between
two points of the grid is 10 m. The heights are for the ground, without
buildings and trees. This data set is for the USA only. For other areas, use
the `globaldem` service with `demtype=COP30`.

Send an HTTP GET request to this URL:

```
https://portal.opentopography.org/API/usgsdem?datasetName=USGS10m&south=44.939&north=45.009&west=-93.288&east=-93.168&outputFormat=AAIGrid&API_Key=<key>
```

The response is an Arc ASCII grid. The first 6 lines are the header. After the
header, each line is one row of heights in meters. The first row is the north
row. Example of a header:

```
ncols        1296
nrows        756
xllcorner    -93.288
yllcorner    44.939
cellsize     9.2592592592593e-05
NODATA_value -999999
```

### 5.2 Buildings

The Overpass query for the buildings is:

```
[out:json][timeout:120];
(
  way["building"]["building"!="no"](44.939,-93.288,45.009,-93.168);
  relation["building"]["type"="multipolygon"](44.939,-93.288,45.009,-93.168);
);
out geom;
```

The area in an Overpass query has this sequence: south, west, north, east.

### 5.3 Roads

The Overpass query for the roads is:

```
[out:json][timeout:120];
(
  way["highway"~"^(motorway|trunk|primary|secondary|tertiary|unclassified|residential|living_street|motorway_link|trunk_link|primary_link|secondary_link|tertiary_link)$"]["area"!="yes"](44.939,-93.288,45.009,-93.168);
  way["highway"="service"]["service"!~"^(parking_aisle|driveway|drive-through)$"]["area"!="yes"](44.939,-93.288,45.009,-93.168);
);
out geom;
```

The query gets all the public roads and the service roads. It does not get
driveways, parking aisles, drive-throughs, footways and cycleways.

### 5.4 Overpass servers

All three servers have the same data for the world.

| Server         | URL                                                       |
|----------------|-----------------------------------------------------------|
| Main server    | `https://overpass-api.de/api/interpreter`                 |
| private.coffee | `https://overpass.private.coffee/api/interpreter`         |
| VK Maps        | `https://maps.mail.ru/osm/tools/overpass/api/interpreter` |

The main server is frequently busy. Then it sends HTTP 504.

To send a query, use an HTTP POST request. Put the query in the field `data`.
Example:

```bash
curl -sS --compressed -A "GopherMapViewer/0.1 (CSCI 3081 class project)" \
  --data-urlencode 'data=<query>' \
  -o apps/MapViewer/data/osm/buildings_44.939_-93.288_45.009_-93.168.json \
  https://maps.mail.ru/osm/tools/overpass/api/interpreter
```

You can also use an HTTP GET request with `?data=<query>`. Encode the query
for a URL.

CAUTION: The public Overpass servers are for a small number of requests. Do
not send requests in a loop. If a server sends HTTP 429 or 504, wait before
the next request.

## 6. How the viewer uses the files

### 6.1 File selection

When you open the 3D view, the viewer calculates the area of the scene. Then
it finds a file for each type of data:

1. If one file covers all of the scene, the viewer uses that file.
2. If more than one file covers all of the scene, the viewer uses the file
   with the smallest area.
3. If a file covers only a part of the scene, the viewer uses the file with
   the largest overlap. The HUD shows a message.
4. If no file covers a part of the scene, the viewer calls the fetcher. If
   the fetcher gives no data, the HUD shows the area to download.

The viewer reads each file one time for each 3D session. A change of the
exaggeration does not cause the viewer to read the files again.

### 6.2 Terrain

- The header keys can have uppercase letters or lowercase letters.
- The viewer replaces each `NODATA_value` cell with the height of the nearest
  cell that has data.
- Outside the grid, the viewer uses the height at the nearest edge of the grid.

### 6.3 Buildings

- Each closed way is one building.
- A multipolygon relation can have holes, for example courtyards. The viewer
  connects the `outer` ways to make outlines. It connects the `inner` ways to
  make holes.
- The roof is flat. The building stands on the lowest point of the ground
  below it.

The viewer uses the OSM tags to calculate the height of a building:

| Tag               | Height                                                   |
|-------------------|----------------------------------------------------------|
| `height`          | The value in meters (`12`, `12 m`) or in feet (`40'`, `40 ft`). |
| `building:levels` | The number of levels × 3 m.                              |
| No tag            | 8 m.                                                     |

If a building has the two tags, the viewer uses `height`. If a value has more
than one part (`12;15`), the viewer uses the first part.

### 6.4 Roads

- Each way with a road class from 5.3 is a road.
- Each OSM node of a road is a node of the graph. The node has the OSM ID,
  the latitude and the longitude.
- Each pair of adjacent nodes on a road is an edge of the graph. The edges
  have no direction.
- Roads that have a node in common connect at that node.
- The viewer shows each road as a band on the ground. The band is 12 m wide
  for a motorway and 4 m wide for a service road.

## 7. Fetchers

### 7.1 Rules

`Fetchers.cpp` has three functions. At this time, each function returns an
empty string.

```cpp
std::string fetchDem(const geo::LatLonBox& box, std::string& error);
std::string fetchOsmBuildings(const geo::LatLonBox& box, std::string& error);
std::string fetchOsmRoads(const geo::LatLonBox& box, std::string& error);
```

Obey these rules when you write the functions:

1. Send the request from section 5 for the area `box`.
2. Return the body of the response. Do not change the body.
3. If the request fails, return `""`. Put a short message in `error`. The HUD
   shows this message.
4. Do not write files. The viewer saves the response.
5. Do not call OpenGL functions.

CAUTION: Do not commit the real API key to git.

The viewer calls the fetchers in the main thread. The window shows a message
while a fetcher runs.

### 7.2 Changes to WebService

| Change                                           | Reason |
|--------------------------------------------------|--------|
| Return the HTTP status code.                     | The fetcher must show a message for 204, 401, 429 and 504. |
| Set `CURLOPT_TIMEOUT` to 60 s.                   | A slow server must not stop the app for a long time. |
| Set `CURLOPT_NOSIGNAL` to 1.                     | The app downloads tiles in more than one thread. |
| Set `CURLOPT_ACCEPT_ENCODING` to `""`.           | The server then compresses the response. The download is faster. |
| Send a `User-Agent` header with the name of the app. | The Overpass servers ask for this header. |
| Do not write the API key to the terminal.        | `WebService::get` writes the full URL to the terminal. The URL has the key. |
