#!/usr/bin/env bash
# Downloads the data files for the MapViewer 3D view. See README.md.
#
# Put this script in the root folder of the repository. Run it from there:
#   export OPENTOPO_API_KEY=your-key
#   ./fetch_prototype_data.sh             # terrain, buildings, roads
#   ./fetch_prototype_data.sh --imagery   # also the imagery tiles
#   ./fetch_prototype_data.sh --check     # only examine the files
#
# The script keeps the files that exist. Delete a file to download it again.
# The script uses https://wiki.openstreetmap.org/wiki/Overpass_API to get the buildings and roads. 
# It uses https://opentopography.org/ to get the terrain.

set -euo pipefail

# The area: south, west, north, east (degrees).
S=44.939; W=-93.288; N=45.009; E=-93.168

DATA=data
TERRAIN=$DATA/terrain/dem_USGS10m_${S}_${W}_${N}_${E}.asc
BUILDINGS=$DATA/osm/buildings_${S}_${W}_${N}_${E}.json
ROADS=$DATA/osm/roads_${S}_${W}_${N}_${E}.json

# Overpass queries. The script puts the area in place of BBOX.
BUILDINGS_QUERY='[out:json][timeout:120];(way["building"]["building"!="no"](BBOX);relation["building"]["type"="multipolygon"](BBOX););out geom;'
ROADS_QUERY='[out:json][timeout:120];(way["highway"~"^(motorway|trunk|primary|secondary|tertiary|unclassified|residential|living_street|motorway_link|trunk_link|primary_link|secondary_link|tertiary_link)$"]["area"!="yes"](BBOX);way["highway"="service"]["service"!~"^(parking_aisle|driveway|drive-through)$"]["area"!="yes"](BBOX););out geom;'

# The script tries the servers in this order. Set OVERPASS_URL to use only one server.
OVERPASS_SERVERS=${OVERPASS_URL:-"https://overpass-api.de/api/interpreter
https://overpass.private.coffee/api/interpreter
https://maps.mail.ru/osm/tools/overpass/api/interpreter"}

# The Overpass servers reject a request (HTTP 406) if it does not identify the app.
CONTACT_URL="https://github.com/csci-3081-20-f26/project-dev"
USER_AGENT="GopherMapViewer/0.1 (CSCI 3081 Gopher GIS; +$CONTACT_URL)"

# If a server was busy (HTTP 429 or 504, or an answer that is not complete), wait this many seconds and try all the servers one more time.
RETRY_WAIT=60

fail() { echo "ERROR: $*" >&2; exit 1; }

# Prints the number of elements in an Overpass response. Fails if the file is not complete Overpass JSON.
check_json() {
    python3 -I -c '
import json, sys
try:
    d = json.load(open(sys.argv[1]))
except ValueError:
    sys.exit("not valid JSON (an error page or a download that stopped)")
if "elements" not in d: sys.exit("not an Overpass response")
if d.get("remark"): sys.exit("not complete: " + d["remark"][:100])
print(len(d["elements"]), "elements")' "$1"
}

download_terrain() {
    [ -n "${OPENTOPO_API_KEY:-}" ] || fail "Set OPENTOPO_API_KEY. See README.md, 4.1."
    echo "terrain: downloading..."
    local url="https://portal.opentopography.org/API/usgsdem?datasetName=USGS10m&south=$S&north=$N&west=$W&east=$E&outputFormat=AAIGrid&API_Key=$OPENTOPO_API_KEY"
    local code
    code=$(curl -sS --compressed --connect-timeout 30 -m 300 -o "$TERRAIN" -w '%{http_code}' "$url") || code="no connection"
    if [ "$code" != 200 ]; then
        echo "  $(head -c 200 "$TERRAIN" 2>/dev/null)"
        rm -f "$TERRAIN"
        fail "terrain: HTTP $code. (401: the API key is not correct. 204: no data for the area.)"
    fi
}

# download_overpass format: <name> <query> <file>
# sends the query to each server until one gives the data.
download_overpass() {
    local name=$1 query=${2//BBOX/$S,$W,$N,$E} file=$3
    local round server code reason busy=0 refused=0
    for round in 1 2; do
        for server in $OVERPASS_SERVERS; do
            echo "$name: asking $server ..."
            code=$(curl -sS --compressed --connect-timeout 30 -m 300 \
                -A "$USER_AGENT" -H "Referer: $CONTACT_URL" \
                -o "$file" -w '%{http_code}' \
                --data-urlencode "data=$query" "$server") || code=000
            case "$code" in
                200) reason=$(check_json "$file" 2>&1) && return
                     reason="the answer is $reason"; busy=1 ;;
                406) reason="HTTP 406: the server refused the request headers"; refused=1 ;;
                429) reason="HTTP 429: too many requests from your connection"; busy=1 ;;
                504) reason="HTTP 504: the server is busy"; busy=1 ;;
                000) reason="no connection to the server (see the curl message above)" ;;
                *)   reason="HTTP $code: $(head -c 120 "$file" 2>/dev/null | tr '\n' ' ')" ;;
            esac
            echo "  $reason"
            rm -f "$file"
        done
        # Try one more time only if a server was busy (504).
        [ "$round" = 1 ] && [ "$busy" = 1 ] || break
        echo "$name: the servers are busy. Waiting $RETRY_WAIT seconds..."
        sleep "$RETRY_WAIT"
    done
    [ "$refused" = 0 ] || echo "For HTTP 406, set CONTACT_URL in this script to a real web address." >&2
    [ "$busy" = 0 ] || echo "The servers are busy. Wait a few minutes and run the script again." >&2
    fail "$name: no Overpass server gave a complete answer."
}

# Using the tiles that are compatible with the 3D data from OSM.
download_imagery() {
    local x y file
    for y in 11789 11790 11791 11792; do
        for x in 7896 7897 7898 7899 7900; do
            file=$DATA/imagery/15-$y-$x.jpg
            [ -s "$file" ] || curl -fsS --connect-timeout 30 -m 60 -A "$USER_AGENT" -H "Referer: $CONTACT_URL" -o "$file" \
                "https://server.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer/tile/15/$y/$x" \
                || fail "imagery tile 15/$y/$x"
        done
    done
    echo "imagery: 20 tiles"
}

check_files() {
    local ok=1
    if head -n 1 "$TERRAIN" 2>/dev/null | grep -qi '^ncols'; then
        echo "terrain:   OK ($(wc -l < "$TERRAIN") lines)"
    else
        echo "terrain:   NOT OK (missing, or not an Arc ASCII grid)"; ok=0
    fi
    local name file result
    for name in buildings roads; do
        file=$BUILDINGS; [ "$name" = roads ] && file=$ROADS
        result=""
        if [ -s "$file" ] && result=$(check_json "$file" 2>&1); then
            printf '%-10s OK (%s)\n' "$name:" "$result"
        else
            printf '%-10s NOT OK (%s)\n' "$name:" "${result:-missing}"; ok=0
        fi
    done
    [ "$ok" = 1 ] || fail "some files are not correct. Delete them and run the script again."
    echo "All data is in place."
}

# --- Start -------------------------------------------------------------------
case "${1:-}" in ""|--imagery|--check) ;; *) fail "Unknown option $1. Use --imagery or --check." ;; esac
[ -d src ] || fail "Run the script from the root folder of the repository."
mkdir -p "$DATA/terrain" "$DATA/osm" "$DATA/imagery"

if [ "${1:-}" != "--check" ]; then
    [ -s "$TERRAIN" ]   || download_terrain
    [ -s "$BUILDINGS" ] || download_overpass buildings "$BUILDINGS_QUERY" "$BUILDINGS"
    [ -s "$ROADS" ]     || download_overpass roads "$ROADS_QUERY" "$ROADS"
    [ "${1:-}" != "--imagery" ] || download_imagery
fi
check_files
