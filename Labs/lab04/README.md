# Lab 04 - Web Services

In this lab, your goal is to create a program that uses a web service.

## What You Will Learn

1. How to call a web service to get information using Swagger and C++.
2. Understanding how a facade is used to simplify complex systems.
3. How to query json objects for information returned from a REST web service.
4. How to use the std::vector<T> class for dynamically sized arrays.
5. How to get real-time transit information about buses and trains.

### What's in lab04

- Makefile:
  - This makefile is provided to automate the building of your project. You should not need to edit this file.

- WebService.h / WebService.cpp
  - These files contains the code for simplified web service calls. You can create a web service by passing in the base url for the service (`WebService webService("https://my-api.com")`). You can call specific methods by using `webService.get("/path/to/method")`. You should not need to edit this file.

- TransitService.h / TransitService.cpp
  - These files contain the following classes:
    - `TransitService` - Implements the TransitService interface and calls the Metro Transit web service directly.

- main.cpp:
  - This file contains the main application that handles the core logic of the program. You may edit this if you need.

### What You Will Edit

- main.cpp
  - You will edit parts of this file, specifically the `realTimeInfo(...)` function for **Milestone 3**.

- TransitService.h
  - You will shouldn't need to edit this file.

- TransitService.cpp
  - You will need to edit this file for **Milestone 2** and **Milestone 3**.

### Goal

<hr>

  **The primary goal of this lab is to become familiar with calling web services and using the proxy pattern to remove the expensive overhead of repeating api calls and restricting access to data.**

<hr>

### Getting Started

Navigate to lab04 in one of the development environments (VOLE / CSE Labs, SSH).

You can run the program with the following commands using the provided `Makefile`.

```bash
% cd lab04
% make
% ./transit_service
```

If you are running this on your own Linux environment, you make sure to have the headers for `libcurl` installed. For Ubuntu/Debian based distros, the package should be called `libcurl4-openssl-dev`, and for Fedora based distros, the package should be called `libcurl-devel`. 

When you run, you will see the following output:

```bash
Options:
-------------------
0: Exit
1: List Agencies
2: List Routes
3: Real-Time Info

Enter a selection:
```

Enter `1` and you should see the following output, which is a call to the MetroTransitAPI (https://svc.metrotransit.org/swagger/index.html):

```bash
Enter a selection: 1

[ Web Service Call: https://svc.metrotransit.org/nextrip/agencies ]
0: Metro Transit
2: Met Council
3: Minnesota Valley
4: Maple Grove
5: Plymouth
6: SouthWest Transit
10: Airport (MAC)
11: University of Minnesota
16: St Cloud Metro Bus

<Press enter to continue>
```

In this assignment, you will implement options 2 (Milestone 2) and 3 (Milestone 3).

You are now ready to begin the Lab!

## Using an API

### Milestone 1 - Swagger

Swagger is a useful interface for accessing and trying out existing REST APIs. The first milestone for this lab entails exploring the Metro Transit NexTrip API. Navigate to the following URL:

- [Metro Transit - NexTrip API](https://svc.metrotransit.org/swagger/index.html)

Investigate the various API calls. Click on “GET /nextrip/agencies”. What is the id for the agency for the “University of Minnesota”. Most likely this represents buses like the Campus Connector.

Use the Swagger interface to find a stop for a place of interest ([Twin Cities Metropolitan Area Transit System Map](https://www.metrotransit.org/media/ns4igy1k/metro_diagrammap.pdf)) finding a route, direction and place_code. The following three API calls should help:

- `/nextrip/routes`
- `/nextrip/directions/{route}`
- `/nextrip/stops/{route}/{direction}`

---

For **Milestone 1** you should be able to understand and make web service calls using the Swagger interface. You should be able to execute calls to find the places of interest in the Metro Transit system.

---

### Milestone 2 - List Routes

For this milestone, you will use your C++ program to list the routes in our system. This is option two in our program:

```bash
Options:
-------------------
0: Exit
1: List Agencies
2: List Routes
3: Real-Time Info

Enter a selection: 2
```

To get this to work, you will need to call the correct web services similar to the `MetroTransitAPI::getAgencies()` method. It will be a different web service call and you will need to get the data using a json object returned from the web service.

The json object can be accessed several ways (similar to lists and dictionaries in python):

- As an array: `json[5]`
- As a dictionary object: `json["route_id"]`
- As a int: `json["agency_id"].get<int>()`
- As a float: `json["agency_id"].get<float>()`
- As a string: `json["agency_name"].get<std::string>()`
- As debug output: `std::cout << json << std::endl;`
- As a hierarchical object (combining the above): `json[2]["departures"][0]["date"].get<std::string>()`

[MetroTransitAPI::getAgencies()](TransitService.cpp#L6) shows an example of how to call the web service and how access the json object.

Once you implement the `MetroTransitAPI::getRoutes()` method, option 2 should work.

---

For **Milestone 2** your output should look something like the following:

```bash
Enter a selection: 2

[ Web Service Call: https://svc.metrotransit.org/nextrip/routes ]
901: METRO Blue Line
902: METRO Green Line
905: METRO Gold Line
904: METRO Orange Line
425: Orange Link
903: METRO Red Line
921: METRO A Line
922: METRO B Line
923: METRO C Line
924: METRO D Line
906: Airport Shuttle
2: Route 2
3: Route 3
4: Route 4
...
```

---

### Milestone 3 - Get Real-Time Info

For this milestone, you will use your C++ program to list the routes in our system. This is option three in our program:

```bash
Options:
-------------------
0: Exit
1: List Agencies
2: List Routes
3: Real-Time Info

Enter a selection: 3
```

To do this, you will implement the rest of the MetroTransitAPI methods in [TransitService.cpp](TransitService.cpp#L33). You will also need to modify the `realTimeInfo(...)` method in the [main.cpp](main.cpp#L69).

---

For **Milestone 3** your output should look similar to the following (It is okay if `[ Web Service Call: ... ]` is part of the output):

```bash
Enter a selection: 3

Enter a route: 14

Available Directions:
-----------------------
0: Southbound
1: Northbound

Enter a direction: 1

Available Stops:
-----------------------
RBTC: Robbinsdale Transit Center
36NB: Noble Ave and 36th Ave
...

Enter a stop: 36NB

Nobel Ave and 36th Ave
-----------------------
Latitude: 45.020797
Longitude: -93.338913
Next Departure: 12 Min
```

