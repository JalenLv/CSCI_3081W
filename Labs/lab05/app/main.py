from contextlib import asynccontextmanager
import os

########################################################################
# FASTAPI ARCHITECTURE
########################################################################
from fastapi.staticfiles import StaticFiles
from fastapi.requests import Request
from fastapi import FastAPI
from starlette.middleware.cors import CORSMiddleware

########################################################################
# GLOBALS
########################################################################
from app.deps.db_pool import close_pool, create_pool
from app.routes import (
	db
)

########################################################################
# USER DEFINED
########################################################################
@asynccontextmanager
async def lifespan_dependencies(app: FastAPI):
    app.state.db_pool = await create_pool(os.environ)
    try:
        yield
    finally:
        await close_pool(app.state.db_pool)

def create_app() -> FastAPI:
    app = FastAPI(lifespan=lifespan_dependencies)

    # Include routers
    app.include_router(db.router)

    # Static files
    app.mount("/assets", StaticFiles(directory="./app/ui/assets/", check_dir=False), name="asset")
    app.mount("/uploads", StaticFiles(directory="uploads", check_dir=False), name="uploads")


    # Middleware
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    return app

app = create_app()

@app.get("/")
async def read_root(request: Request):
    
    return {"status": "ok"}

@app.get("/health")
async def read_health(request: Request):
    
    return {"status": "ok"}
