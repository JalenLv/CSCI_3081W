# Lab 05: Data Layer Integration & Querying

## Goal

In this lab you will connect a FastAPI service to PostgreSQL, expose database-backed endpoints, and verify each endpoint with a small custom `httpx` command line client. By the end of the lab, your API should be able to read, create, update, and delete records while returning JSON responses that prove the database changed.

## What You Will Learn

1. How to run PostgreSQL and FastAPI with Docker Compose.
2. How to inject database configuration with environment variables.
3. How to initialize tables and seed data from SQL.
4. How to organize a simple data layer with DTOs, repositories, services, and routes.
5. How to call an API from a custom Python `httpx` client.
6. How to verify CRUD behavior with read-after-write checks.

## What's in lab05

- `app/main.py`: FastAPI application factory and startup database pool wiring.
- `app/routes/db.py`: API routes for database-backed resources.
- `app/db/dtos.py`: Pydantic request and response data models.
- `app/db/repos/`: Repository classes that run SQL queries.
- `app/db/service/db_table_service.py`: Service layer that combines repository results into API responses.
- `app/deps/db_pool.py`: Async PostgreSQL connection pool dependency.
- `sql/v1.sql`: Schema and seed data for students, courses, and enrollments.
- `docker-compose.yml`: Local PostgreSQL and API runtime.
- `Dockerfile.db` and `Dockerfile.server`: Container build files.
- `requirements.txt`: Python dependencies.

## Python venv

Create and activate a local development environment from the `lab05` directory:

```bash
cd lab05
python3 -m venv .venv/dev
source .venv/dev/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

If your shell prompt does not show the virtual environment, confirm that `which python` points inside `lab05/.venv/dev`.

The expected workflow for this lab is:

- **Milestone 1** - Launch a Database and the API with Docker Compose.
- **Milestone 2** - Create and Load the Database.
- **Milestone 3** - Make API Calls.
- **Milestone 4** - Explore a custom CLI client.

Run all commands from the `lab05` directory unless a step says otherwise.

## Milestone 1: Launch the Database and API using Docker Compose

### Docker Compose

Docker compose is a simple application that allows us to manage a simple server that runs multiple containers simultaneously. Docker Compose is similar to a Dockerfile, but uses the YAML format for configuration. We can bring an entire environment up or down with the simple `docker compose up` or `docker compose down` commands. In this case, we can avoid starting and stoping the services separately. In general, docker compose allows us to simplify maintanence on a single machine. For more complicated, multi-server systems for test, stage or production, a container maintanence tool like Kubernates would scale better. However, Docker Compose is a great option for development environments.

Open up [docker-compose.yaml](docker-compose.yaml). You will notices that two containers are defined (`db: lab05-postgress` and `api: lab05-fastapi`):

```
services:
  db:
    image: postgres:16-alpine
    container_name: lab05-postgres
    environment:
        ...
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data
    ...

  api:
    image: python:3.12-slim
    container_name: lab05-fastapi
    depends_on:
      db:
        condition: service_healthy
    environment:
        ...

```

Notice that we can define base images, enable ports, environment variables specific to containers, volumes (persistent storage locations), and other attributes similar to a `Dockerfile`.

When we call `docker compose up` for this application, we are loading up all the containers, specifically the `db` and the `api`.

### Launch the Postgress database and API

Start the database and API:

```bash
docker compose up --build
```

<details>
<summary>Troubleshooting</summary>

```text
(dev) docker compose up --build
Cannot connect to the Docker daemon at unix:///var/run/docker.sock. Is the docker daemon running?
```

**SOLUTION**:

```
sudo systemctl start docker
```

-or-

```
open -a docker
```

```
docker info
```

</details>

In a second terminal, verify both containers are running:

```bash
docker compose ps
```

The API should be available at:

```text
http://localhost:8000
```

Check the root health response:

```bash
curl http://localhost:8000/
```

Expected response:

```json
{ "status": "ok" }
```

To stop the lab environment:

```bash
docker compose down
```

To remove the database volume and reset all data (Only do this command if you want to restart and remove all persistent database data):

```bash
docker compose down -v
```

### Configuring the Environment (If you would like to change the environment settings.)

The FastAPI app reads database settings from environment variables:

```text
DB_HOST
DB_PORT
DB_NAME
DB_USER
DB_PASSWORD
```

For local development outside Docker, create `.env` from the example values and use the PostgreSQL service exposed on port `5432`:

```bash
cp .env.example .env
```

Use these values if `.env.example` is empty:

```text
DB_HOST=localhost
DB_PORT=5432
DB_NAME=lab05
DB_USER=lab05
DB_PASSWORD=lab05
```

When running inside Docker Compose, the API container should use the database host name `db`:

```text
DB_HOST=db
DB_PORT=5432
DB_NAME=lab05
DB_USER=lab05
DB_PASSWORD=lab05
```

If you update `docker-compose.yml`, make sure the `api` service receives these variables and still depends on the healthy `db` service.

---

For **Milestone 1** you should be able to start and stop both the `db` and the `api` with `docker compose` and check the health of the api. If the `api` is healthy, you have passed this milestone.

---

## Milestone 2: Create and Load the Database.

### Use the postgres client to load the data into the Database

After Docker is running, load the schema and seed rows:

```bash
docker compose exec -T db psql -U lab05 -d lab05 < sql/v1.sql
```

**Note:** The command `docker compose exec -T db <cmd>` will run the `psql -U lab05 -d lab05 < sql/v1.sql` on the `db` container. This creates the tables and loads the data in `sql/v1.sql`. You can use `docker compose exec -T <container> <cmd>` to run commands on the service.

Confirm the tables exist:

```bash
docker compose exec db psql -U lab05 -d lab05 -c "\dt"
```

Confirm seed data loaded:

```bash
docker compose exec db psql -U lab05 -d lab05 -c "SELECT id, first_name, last_name, major FROM students ORDER BY id;"
```

### Use DBeaver to Query the Database

If you do not already have DBeaver, the link to the website can be found [here](https://dbeaver.io/). Follow the download and installation instructions on the Download page.

Open up **DBeaver** and create a new Database Connection:

- File -> New -> Database Connection -> Postgres SQL (Standard Driver)
- Set the following:
  - Host: localhost
  - Database: lab05
  - Username: lab05
  - Password: lab05
- Click Finish

Now you can navigate to the `Schemas/public/Tables` folder and query the tables similar to the Database Workshop from class.

Open a "New SQL Script" and run the following query:

```
SELECT id, first_name, last_name, major FROM students ORDER BY id;
```

You should see the following data:

```
id|first_name|last_name|major           |
--+----------+---------+----------------+
 1|Ada       |Lovelace |Computer Science|
 2|Grace     |Hopper   |Computer Science|
 3|Katherine |Johnson  |Mathematics     |
 4|Dorothy   |Vaughan  |Mathematics     |
 5|Alan      |Turing   |Computer Science|
```

---

For **Milestone 2** you should be able to access and run queries on the database through the command line and through DBeaver.

---

## Milestone 3: Make API Calls

For Milestone 3, we will explore several methods to make API calls to the `api` service container. The code for the API (the `./app/` folder) uses the repository pattern to handle CRUD (**C**reate, **R**ead, **U**pdate, **D**elete) operations.

**Note:** Do not worry too much about how this is implemented. The code is fairly complicated. It is not necessary to use the repository pattern, but it is one standard that is implemented in the industry, so it is worth exploring. In the future, we will create our own APIs and learn how to access databases. However, at this time, you are welcome to explore the code and make changes, but understanding the code is not a requirement for this lab.

### Use Swagger

We will start by using the FastAPI Swagger environment. This debugging tool is built into the FastAPI python library. Since we are using this for our API, it comes for free when we setup our API endpoints.

Navigate to the following:

- http://localhost:8000/docs

You will see a list of web service endpoints you can call.

**Try out the following:**

- `GET /db/students` - List the students in the database.
- `POST /db/students` - Add a new student.
- `GET /db/students` - Verify the new student appears.

**Checking Docker Logs**

What if one of these requests fails, or the API doesn't return what you expected? How do you figure out what went wrong?

That's where Docker logs come in! Even if your Docker terminal looks quiet, PostgreSQL or FastAPI might be reporting useful errors in the background.They are collected in the container logs

You can check those messages by running the following command:

- `docker compose logs -f` — Watch live logs from both containers (PostgreSQL and FastAPI).

**Create a course and add a student:**

Use the Swagger environment to create a new course and enroll the student you just created into the course. You will need to do the following:

1. Get the id of the student you created.
2. Create a course.
3. Get the id of the course you created.
4. Enroll the student in the course using the id of each.

### Use Curl

You can use the command line to run these commands. Notice that when you run a method using Swagger, it also gives you the Curl command for calling the web service. Here are examples below:

<details>
<summary>What is curl?</summary>

`curl` is a command-line tool that lets you communicate with servers using different protocols, such as HTTP, HTTPS, FTP, and SFTP.

For example, when you enter `google.com` in your browser, your browser sends an HTTP request to Google's server and receives a response. `curl` lets you do something similar directly from your terminal, without opening a browser.

The following commands show how to use `curl` to send HTTP requests.

</details>

Listing Students:

```
curl -X 'GET' \
  'http://localhost:8000/db/students' \
  -H 'accept: */*'
```

Adding a new Student:

```
curl -X 'POST' \
  'http://localhost:8000/db/students' \
  -H 'accept: */*' \
  -H 'Content-Type: application/json' \
  -d '{
  "first_name": "Bob",
  "last_name": "Smith",
  "email": "bob@example.edu",
  "major": "None",
  "credits": 0,
  "active": true
}'
```

Try calling the web service using the `curl` application.

---

For **Milestone 3** show that you created a student and enrolled them in a new course using the API (Swagger or curl).

---

## Milestone 4 - Use a Custom Python Client

For Milestone 4, we will use a custom Python CLI client that uses `httpx`. In the `lab05` directory, there is a `client.py`. You can use this client to call the API using Python. Explore the code and understand where and how the API is being called.

**Try out the client:**

```bash
python client.py students list
python client.py students create --first-name Mae --last-name Jemison --email mae.jemison@example.edu --major Engineering --credits 64
python client.py students update 6 --first-name Mae --last-name Jemison --email mae.jemison@example.edu --major "Computer Science" --credits 80 --active true
python client.py courses list
python client.py enrollments list
```

**Note:** Use `--base-url` before the resource name if your API is not running at `http://localhost:8000`.

---

For **Milestone 4** verify that the client works as planned and fix any issues you run into. You can use the Postman or Insomnia application to help you create Python code or look at other `httpx` examples.

---
