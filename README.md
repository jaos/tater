# tater

Simple scripting language based upon the great work of [Robert Nystrom](https://craftinginterpreters.com/)

`example.tot`:

```tater
#!/usr/bin/tater

type WorkSites {
    let RESIDENTIAL = 0x1;
    let COMMERCIAL = 0x2;
    let INDUSTRIAL = 0x4;
}

type WorkCounter {
    let name;
    let counter = 0;
    fn init(name) {
        self.name = name;
    }

    fn work(locations) {
        for (let i = 0; i < locations.len(); i++) {
            self.counter++;

            let work = locations[i];
            print(self.name + ", location counter " + str(self.counter));
            print(work);
            let start_point = work["start"];
            let end_point = work["end"];

            switch (work["site"]) {
                case WorkSites.RESIDENTIAL: {
                    print("setting up residential drone.");
                    // start flying
                    while (start_point[0] < end_point[0]) {
                        start_point[0]++; # simulation
                    }
                }
                case WorkSites.COMMERCIAL: {
                    # more here
                }
                case WorkSites.INDUSTRIAL: {
                    # more here
                }
            }
        }
    }
}

let b = true;
let wc = WorkCounter("phase1");
let workorder = [
    {"start": [900,100], "end": [920, 80], "site": WorkSites.RESIDENTIAL},
    {"start": [1_360,1_800], "end": [1_390, 2_000], "site": WorkSites.COMMERCIAL},
    {"start": [3_490,3_400], "end": [3_700, 3_550], "site": WorkSites.INDUSTRIAL}
];

wc.work(workorder);
```

## Building

Requires:

* libreadline
* libcheck (build time, unit tests)

```sh
meson setup build -Ddebugging=enabled -Db_coverage=true --prefix=/usr --sysconfdir=/etc
meson compile -C build
```

## Testing

```sh
meson test -C build && ninja coverage-html -C build
```

Run the REPL

```sh
meson devenv -C build ./src/tater
```

Run the benchmark

```sh
meson devenv -C build ./src/tater $PWD/t/bench.tot
```

## Translations

```sh
# edit po/LINGUAS etc
meson compile tater-pot -C build
meson compile tater-update-po -C build
```
