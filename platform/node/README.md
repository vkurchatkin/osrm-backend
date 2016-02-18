# node-osrm

Provides read-only bindings to the [Open Source Routing Machine - OSRM](https://github.com/Project-OSRM/osrm-backend), a routing engine for OpenStreetMap data implementing high-performance algorithms for shortest paths in road networks.

# Depends

 - Node.js v4.x
 - Modern C++ runtime libraries supporting C++11 or C++14

C++11 capable platforms include:

  - Mac OS X >= 10.8
  - Ubuntu Linux >= 14.04 or other Linux distributions with g++ >= 4.8 toolchain (>= GLIBC_2.17 from libc and >= GLIBCXX_3.4.17 from libstdc++)

An installation error like below indicates your system does not have a modern enough g++ toolchain:

```
Error: /usr/lib/x86_64-linux-gnu/libstdc++.so.6: version `GLIBCXX_3.4.17' not found (required by /node_modules/osrm/lib/binding/osrm.node)
```

If you are running Ubuntu older than 14.04 you can easily upgrade your g++ toolchain like:

```
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install g++-4.8
```


# Installing

By default, binaries are provided for:

 - 64 bit OS X and 64 bit Linux
 - Node v4

On those platforms no external dependencies are needed.

Just do:

    npm install osrm

However other platforms will fall back to a source compile: see [Source Build](#source-build) for details.


# Testing

Run the tests like:

    npm test


# Usage

See the `example/server.js` and `test/osrm.test.js` for examples of using OSRM through this Node.js API.

# Setup

The `node-osrm` module consumes data processed by OSRM core.

You can use the test data provided in this repo at ../../test/data

```js
// Note: to require osrm locally do:
// require('./lib/osrm.js')
var OSRM = require('osrm')
var osrm = new OSRM("../../test/data/berlin-latest.osrm");

osrm.locate([52.4224,13.333086], function (err, result) {
  console.log(result);
  // Output: {"status":0,"mapped_coordinate":[52.422442,13.332101]}
});

osrm.nearest([52.4224, 13.333086], function (err, result) {
  console.log(result);
  // Output: {"status":0,"mapped_coordinate":[52.422590,13.333838],"name":"Mariannenstraße"}
});

var query = {coordinates: [[52.519930,13.438640], [52.513191,13.415852]]};
osrm.route(query, function (err, result) {
  console.log(result);
  /* Output:
    { status: 0,
      status_message: 'Found route between points',
      route_geometry: '{~pdcBmjfsXsBrD{KhS}DvHyApCcf@l}@kg@z|@_MbX|GjHdXh^fm@dr@~\\l_@pFhF|GjCfeAbTdh@fFqRp}DoEn\\cHzR{FjLgCnFuBlG{AlHaAjJa@hLXtGnCnKtCnFxCfCvEl@lHBzA}@vIoFzCs@|CcAnEQ~NhHnf@zUpm@rc@d]zVrTnTr^~]xbAnaAhSnPgJd^kExPgOzk@maAx_Ek@~BuKvd@cJz`@oAzFiAtHvKzAlBXzNvB|b@hGl@Dha@zFbGf@fBAjQ_AxEbA`HxBtPpFpa@rO_Cv_B_ZlD}LlBGB',
      route_instructions:
       [ ... ],
      route_summary:
       { total_distance: 2814,
         total_time: 211,
         start_point: 'Friedenstraße',
         end_point: 'Am Köllnischen Park' },
      alternative_geometries: [],
      alternative_instructions: [],
      alternative_summaries: [],
      route_name:
       [ 'Lichtenberger Straße',
         'Holzmarktstraße' ],
      alternative_names: [ [ '', '' ] ],
      via_points:
       [ [ 52.519934, 13.438647 ],
         [ 52.513162, 13.415509 ] ],
      via_indices: [ 0, 69 ],
      alternative_indices: [],
      hint_data:
       { checksum: 222545162,
         locations:
          [ '9XkCAJgBAAAtAAAA____f7idcBkPGuw__mMhA7cOzQA',
            'TgcEAFwFAAAAAAAAVAAAANIeb5DqBHs_ikkhA1W0zAA' ] } }
  */
});
```

# Source Build

First build osrm-backend and ensure that the `libosrm.pc` file is available by running this command:

    pkg-config libosrm --variable=prefix

You may need to do: `export PKG_CONFIG_PATH=../../build` if you did not install osrm-backend somewhere where
the `libosrm.pc` file can be found by default.

Now you can build `node-osrm`:

    npm install --build-from-source

# Developing

After setting up a [Source Build](#source-build) you can make changes to the code and rebuild like:

    npm install --build-from-source

But that will trigger a full re-configure if any changes occurred to dependencies.

However you can optionally use the Makefile which simplifies some common needs.

To rebuild using cached data:

    make

If you want to see all the arguments sent to the compiler do:

    make verbose

If you want to build in debug mode (-DDEBUG -O0) then do:

    make debug

Under the hood this uses [node-pre-gyp](https://github.com/mapbox/node-pre-gyp) (which itself used [node-gyp](https://github.com/TooTallNate/node-gyp)) to compile the source code.
