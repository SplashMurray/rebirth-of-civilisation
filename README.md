# Rebirth of Civilisation

**2034. Humanity destroys itself. The primates inherit the Earth.**

*Rebirth of Civilisation* is an open-source, turn-based 4X strategy game for Windows in the style of the classic *Civilization* games. It begins in 2032, shortly before humanity wipes itself out in a global nuclear war. Humans are not playable; you lead a band of **chimpanzees, bonobos, orangutans, gorillas, baboons, capuchins, gibbons or snow monkeys**. Survive the Fall, then rebuild civilisation from the ashes. In time, the ruins of the human world give up their secrets to primates advanced enough to understand them.

![Title screen](docs/screenshots/01_title.png)

> This is an independent fan project inspired by the 4X genre. It is not affiliated with or endorsed by Firaxis Games, 2K or the *Civilization* franchise.

## Download and play

* **Download:** open the [Releases](../../releases) page and download `RebirthOfCivilisation-windows-x64.zip`. Every push also produces a build under **Actions → latest run → Artifacts**.
* **Install:** none. Unzip the file and run `RebirthOfCivilisation.exe`. It is one native Win32 program with no runtime dependencies, and it runs on Windows 7 through 11. Keep the `assets` folder next to the .exe - it holds the music and sounds (without it the game simply plays silently).
* **Saves:** stored in `%APPDATA%\RebirthOfCivilisation\savegame.roc`. The game also autosaves every 5 turns.
* **Code signing policy:** see [CODE_SIGNING_POLICY.md](CODE_SIGNING_POLICY.md). Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org) (application in progress; current downloads are unsigned, so Windows 11 Smart App Control may block them).
* **Privacy:** the game never connects to the internet and collects no data. See the privacy policy in [CODE_SIGNING_POLICY.md](CODE_SIGNING_POLICY.md#privacy-policy).

## The story

| Phase | What happens |
|---|---|
| **The last days (Jan 2032 – Aug 2034)** | Six turns of the prologue. Human cities still glow on the horizon and their radios grow more frightening each turn. Your people are still wandering bands: use the time to explore and forage, but keep away from the human cities. |
| **The Fall (August 2034)** | A global nuclear war wipes out humanity completely. Every human city, town, military base, laboratory and power plant becomes a ruin surrounded by radioactive fallout. Units caught in the blasts die. The war plays out as a short animation that fades back to the map by itself (click or press a key to skip it). |
| **The wandering age** | For tens of thousands of years after the Fall the primates are nomads. Roaming bands forage, grow, split and fight off predators while their brains evolve towards speech. Only a species that has evolved **Speech** and discovered **Cultivation** can settle down and found its first city - the great transition from roaming pack to settled people. Each species begins in its own region, spread as far from the others as the land allows, and rival bands give each other room. |
| **The rebirth (Year 1 AF onward)** | Years are counted *After the Fall*. Pressured by a changing world, the primates evolve speech, then advance from fire and stone tools to writing, currency, industry, computing and finally the **Rebirth Project**. |

![The Fall](docs/screenshots/06_nuke_anim.png)

## Features

* **Anti-aliased, illustrated graphics** (GDI+):
  * **Terrain** blends softly from tile to tile, with sandy shores, sunlit shallows, light and shade across the ground, gradient-lit mountains with snow caps, rolling hills, layered conifers, round-crowned woods and jungle with palms. The known world fades into darkness at its edges.
  * **Live water**: glints drift across the sea and surf rolls onto the beaches.
  * **Units** are bronze-rimmed medallions (shields for civilians) in their owner's colour, with an icon for every unit type. They **glide** from tile to tile and **lunge** at their targets. Arrows and stones arc through the air, bullets and shells streak, hits flash and spark, damage numbers float up and destroyed units vanish in smoke.
  * **Cities** grow from huts round a fire, to brick and stone houses with tiled roofs, to smoking factories and glass towers. Walls gain towers, and capitals fly a fluttering pennant.
  * **Screens** have rounded, shaded panels and buttons, illustrated portraits of each primate, and an animated title scene: a ruined city at sunset under a mushroom cloud.
* **Sound and music.** Music changes with your story: a sombre theme on the title screen, drums and flutes in the wandering age, fuller pieces as civilisation grows, battle music while you are fighting a war, a lament for the Fall and a fanfare for victory - crossfading as the mood changes. Every fight sounds the part (swords, spears, bows, slings, muskets, catapults and cannon, impacts, falling cities and sinking ships), as do footsteps and hooves, ships, gatherers at work, new technologies, evolution, cities, trade, wars, disasters and the bombs. Ambient sound follows what is on screen: surf by the sea, birds and jungle in the forests, wind over snow, mountains and fallout. Music, sound effects and ambience each have a volume control (in the menu; music and effects also on the title screen). The music and sounds are from the open-source game [0 A.D.](https://play0ad.com/) by Wildfire Games, used under [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) - see [assets/audio/CREDITS.txt](assets/audio/CREDITS.txt).
  * **Animations** can be turned off in the menu for slow computers. The terrain is cached in chunks, so scrolling the map stays smooth.
* **Hex map**, randomly generated, with grassland, plains, desert, tundra, snow, coast and ocean, plus forest, jungle, hills, mountains and resources. Small (60 × 38), Standard (74 × 46) and Large (84 × 54) sizes with 4, 6 or 8 primate civilisations: about 570 tiles per species, the same room each as in Civilization VI.
* **Eight playable primates**, each with its own bonuses, a unique unit and a preferred starting terrain (see below).
* **A nomadic beginning.** There are no cities at the start. Each species begins as a **Roaming Band** that forages the tiles around it (one per member, plus one). Foraged land is picked over and recovers slowly, so bands must keep moving to fresh forest, jungle and grassland. Well-fed bands grow to at most 8 members; a band can **split** in two, or turn members into a **war party** or a **scout**. Hungry bands lose members, and predators, droughts, wildfires, sickness and cold strike the wanderers. Settling needs **Speech** (evolution) and **Cultivation** (technology); a band of N members founds a city of size 1 + (N-1)/2.
* **Evolution: a branching tree driven by real pressures.** At the start your species cannot learn deliberately and **earns no science at all** until it evolves an **Enlarged Neocortex**. Four kinds of *evolutionary pressure* fill four separate pools every turn:

  | Pressure | Comes from |
  |---|---|
  | **Social** | Band life (before settling), group living (cities), crowding (population), shared stories (culture) |
  | **Ecological** | Survival, roaming and hunger (before settling), ranging over a large territory, harsh lands (tundra, desert, mountains), the nuclear winter after the Fall |
  | **Conflict** | Being at war, battles fought, feral predators near your settlements or bands |
  | **Mutation** | Fallout in your lands and on your units, plus fading background radiation after the Fall |

  On the Evolution screen (`V`) you **choose** which trait to evolve towards, and it develops once your pools can pay its cost. Every trait needs particular pressures:

  | Strand | Traits (final adaptation in bold) | Mainly needs |
  |---|---|---|
  | Brain and Language (required) | Enlarged Neocortex, Expressive Gestures, Proto-Speech, **Speech** (Tribal techs), **Advanced Speech** (Ancient techs), Abstract Thought (Classical onwards) | Social, Ecological |
  | Tool Brain | Precision Grip, Spatial Reasoning, **Master Toolmakers** (+20% Production) | Ecological |
  | Social Brain | Pair Bonding, Extended Childhood, **Collective Learning** (+Science, +Culture) | Social |
  | Predator Brain | Pack Hunters, Ambush Instinct, **Apex Predators** (+combat, cheaper armies) | Conflict |
  | Mutant Line | Radiation Tolerance, Accelerated Healing, **Fallout-Adapted** (fallout no longer harms your land) | Mutation |

  A species may commit to **only two strands** and can have **only one final adaptation**. Because each strand needs its own pressure, a peaceful species will struggle to become Apex Predators, and only those living beside the irradiated ruins can become Fallout-Adapted. Each primate leans towards one strand (Bonobos and Gibbons Social, Gorillas and Baboons Predator, Snow Monkeys Mutant, the others Tool), and social or vocal species evolve faster. Science earned while you wait for a language trait is stored, not lost.
* **Turn-based economy** with Food, Production, Science and Culture. **Gold is not available at the start.** Your people only learn the value of money when they research **Currency**; from then on cities earn Gold, which buys units and buildings.
* **47 technologies across 9 eras** (Awakening, Tribal, Ancient, Classical, Kingdoms, Reclamation, Industrial, Modern, Rebirth), shown as a scrollable tech tree. You can click any technology to set it as a research goal.
* **Human ruins as a source of technology.** Every ruin holds up to four layers of salvage that unlock as you advance:

  | Layer | Unlocked by | Reward |
  |---|---|---|
  | Scrap | Salvaging | Science, plus Production for your nearest city |
  | Books | Deciphering | A large burst of Science |
  | Machines | Reverse Engineering | A big burst of Science (cities), or Science and a restored war machine (military bases) |
  | Data Archives | Computing | The largest burst of Science |

  Rewards scale with what you are researching, with diminishing returns: the first finds are the richest. Ruins inside your borders also yield Production and Science, and the *Archive of Humanity* and *Salvage Yard* buildings boost them further.
* **Resources revealed by technology.** Deposits only appear on the map once you understand them. Strategic resources inside your borders unlock units and buildings, and the right improvement boosts their yields:

  | Resource | Revealed by | Used for / improvement |
  |---|---|---|
  | Copper | Digging | With Tin: **Bronze Phalanx** (Bronze Working). Mine |
  | Tin | Bronze Working | With Copper: Bronze Phalanx. Mine |
  | Horses | Animal Taming | Beast Riders, Baboon Raiders, Armoured Riders. Pasture |
  | Iron | Smelting | Iron Blades, Armoured Riders, Bombards. Mine. Salvaged **Scrap Metal** from ruins counts as Iron |
  | Coal | Steam Power | Factory, Power Station. Mine |
  | Oil | Combustion | Rebuilt Tanks. Mine |
  | Uranium | Radiology | Exo-Primates. Mine |
  | Gold Ore | Currency | +3 Gold. Mine |
  | Wheat, Rice, Maize | always visible | Crops: a **Farm** on them gives +2 Food |
  | Fruit, Fish, Game, Stone | always visible | Bonus food and production |

  Every start position is guaranteed Copper, Tin, Horses, Iron and Coal within a few tiles, though you still have to claim them.
* **Disasters that drive evolution.** About once every 20–25 turns (at Standard speed) a disaster strikes each species. Which one depends on where it lives, and each adds evolutionary pressure: a burst of points now plus lingering pressure for a few turns.

  | Disaster | Likelier when you have | Damage | Pressure |
  |---|---|---|---|
  | Wildfire | Forest and jungle | Burns a patch of woodland and lumber camps, hurts units | Ecological |
  | Drought | Plains and desert | −33% Food in nearby cities for several turns | Ecological, Social |
  | Flood | Coastal cities | −1 population, halves food stores, washes away improvements | Ecological, Social |
  | Epidemic | Large, crowded cities | Kills population and spreads to neighbouring cities | Mutation, Social |
  | Earthquake | Hills and mountains | Damages city defences, may destroy a building | Ecological, Social |
  | Harsh Winter | Tundra and snow, or the nuclear winter | Halves food stores, frostbite for troops in the open | Ecological |
  | Reactor Leak | Ruined power plants near your cities | Fresh fallout around the ruin | Mutation |
  | Predator Migration | After the Fall | Several feral packs arrive near a city | Conflict |
* **Fallout** damages units and poisons tiles. It fades over time, and Gatherers can clean it up once you research Radiology.
* **Cities** grow, work tiles, expand their borders with culture, build 25 buildings and 6 world wonders, and bombard nearby enemies.
* **28 unit types:** Warbands and Stone Throwers early on, then Salvaged Rifles, Rebuilt Tanks and Exo-Primates. Ranged and siege units, embarkation across water, fortification, terrain defence bonuses and city capture all work as you would expect from the genre.
* **Traditions:** 16 culture-bought policies, similar to *Civilization V*'s social policies.
* **Feral packs:** feral dogs, then mutant hounds, then rogue human war machines roam the wastelands. They take the place of barbarians.
* **Diplomacy:** AI rivals declare war based on their personality and relative strength. You can declare war or sue for peace.
* **AI opponents** that forage, settle, expand, research, excavate ruins, wage war and race you to victory.
* **Naval warfare.** Coastal cities build ships, from Dugout Canoes and War Galleys to Frigates, Ironclads and salvaged human Destroyers and Battleships:

  | Ship | Technology | Strength | Notes |
  |---|---|---|---|
  | Dugout Canoe | Rafts | 6 | Coastal waters only |
  | War Galley | Bronze Working | 12 | Coastal; needs Copper |
  | Archer Galley | Archery | 8 / ranged 11 (2) | Coastal |
  | Caravel | Navigation | 16 | Ocean-going, 5 moves |
  | Frigate | Gunpowder | 22 / ranged 28 (2) | Ocean-going |
  | Salvaged Freighter | Steam Power | 10 | Ocean-going transport, carries 4 |
  | Ironclad | Steam Power | 40 | Ocean-going; needs Coal |
  | Salvaged Destroyer | Combustion | 58 | Ocean-going, 6 moves; needs Oil |
  | Salvaged Battleship | Motorised Warfare | 50 / ranged 68 (3) | Ocean-going; needs Oil |

  Ships sail on water and dock in your own coastal cities, sharing the tile with a land garrison. Melee ships ram ships and embarked units and can storm a coastal city; ranged ships bombard anything in range, including the shore. Embarked land units are weak, so escort them. Coastal military-base ruins can yield a warship raised from the harbour. The AI keeps a small navy in peacetime, scouts unknown waters, and in war hunts enemy ships and bombards or storms coastal cities (Hard and Expert join the attack on their war-plan target).
* **Transports.** Canoes and War Galleys carry 1 land unit, Caravels 2, Frigates 1 and Salvaged Freighters 4. Move a land unit onto your ship's tile to board (or press `K` in a harbour city); it sails with the ship and lands by moving onto an adjacent land tile. Units aboard can't fight, and go down with the ship. The AI ferries Elder Troops to good land across the water, and Hard/Expert AIs ship armies to enemy cities they can't reach on foot.
* **Unit upgrades.** Inside your own territory a unit can be refitted (`U`) as the best unit of its line you can train - Warband → Spear Troop → Iron Blades → Salvaged Rifles → Riflemen → Infantry → Exo-Primates, Stone Thrower → Bow Troop → Crossbows → Field Guns, Siege Sling → Bombard → Field Guns, Beast Riders → Armoured Riders → Rebuilt Tanks, Dugout Canoe → War Galley → Caravel → Ironclad → Salvaged Destroyer, Archer Galley → Frigate → Salvaged Battleship. Unique units follow the line of the unit they replace, and upgrade into your species' unique unit where there is one. With Currency it costs gold; before that, the refit is done inside one of your cities and paid for from that city's stored production. Units keep their health. AIs upgrade too (not on Easy).
* **Diplomacy.** Every species has a diplomatic personality:

  | Primate | Personality | |
  |---|---|---|
  | Chimpanzee | Territorial Politician | Builds coalitions, guards its range jealously, never forgets a slight |
  | Bonobo | Peacemaker | Warm towards almost everyone, loves agreements, forgives quickly |
  | Orangutan | Solitary Sage | Trades knowledge happily but rarely binds itself with pacts |
  | Gorilla | Protector | Slow to anger, fiercely loyal, despises warmongers |
  | Baboon | Opportunist | Respects only strength, demands tribute, may betray a weak friend |
  | Capuchin | Trader | Deals with anyone and grows fond of good trading partners |
  | Gibbon | Faithful Partner | Chooses one close partner for life |
  | Japanese Macaque | Hierarchist | Respects the powerful, looks down on the weak |

  Each civilisation's **opinion** of you is built from visible reasons - personality, fellow apes/monkeys, shared borders, wars, remembered wrongs, gifts, treaties, common enemies, warmongering, nuclear weapons, betrayals - and shown line by line on the Diplomacy screen, from Hostile to Close friends. The **trade screen** lets you bundle gold, gold per turn, resources (a shared strategic or luxury resource counts as the receiver's own for 30 turns), **open borders**, **research pacts** (+15% science each), **declarations of friendship**, **defensive pacts** (an attack on one brings in the other) and declarations of war on third parties; it shows live whether they would accept, and *What would it take?* adds the gold they want. **Borders** are open to everyone until a civilisation discovers **Tribal Law**; from then on they are closed by default, and in peacetime other armies may only enter if it opens its borders to them in a deal (each side opens its own, so it can be one-way). Tribal Law also lets you impose an **embargo**: no gold, gold per turn, resources or research pacts can pass between you and the embargoed civilisation, existing ones end at once, and they will resent it - until you lift it. Making peace brings a **peace treaty** forbidding war for a while; declaring war on a sworn friend is a **betrayal** everyone remembers. You can **denounce** rivals. The AI signs treaties with each other, denounces those it despises, avoids attacking friends, and sends you proposals - peace offers, pacts, resource swaps, requests to join its wars and, from strong aggressive neighbours, demands for tribute.
* **Trade caravans.** With *Currency* your cities train **Caravans**. Move one into one of your cities and choose *Trade Route*: a list shows every city within reach - yours or another species' you have met - with what the route would earn, and the way it would go is drawn on the map. Caravans travel **overland** (up to 15 tiles) or **by sea** between coastal cities (along the coast with *Rafts*, across the ocean with *Navigation*). A route lasts 30 turns on Quick speed (45 on Standard, 68 on Epic), then the caravan comes home to be sent out again; you can watch caravans and cargo boats travelling along your routes.
  * Routes to **other species** earn gold for your city - more for big, distant cities - plus science if they know things you don't, and give their city a third as much gold. Routes between **your own cities** bring food and production. Trading partners like each other more (the Capuchins especially).
  * **Roads** along the way make a route richer (up to +60% on tarmac); the sea pays +25% (+40% with Navigation).
  * You can run one route with Currency, one more each with Navigation, Banking, Industrialisation and Computing, and one for every two Markets.
  * Caravans are civilians: they may cross **closed borders**, but enemies at war with you can kill them in the field. An **embargo** or a **war** ends every route between the two sides at once (the caravans turn back home) and no new ones can start; an enemy army or warship standing on a route **plunders** it for gold.
  * The AI trains caravans, picks the most profitable routes (and prefers friends) and builds roads along them.
* **Roads.** Caravans wear a **Worn Track** along their routes. Gatherers build roads on your land or unclaimed land and upgrade them one level at a time:

  | Road | Needs | Gatherer turns | Move between road tiles | Trade bonus |
  |---|---|---|---|---|
  | Worn Track | - | 2 | two-thirds of the usual cost | +10% |
  | Dirt Road | Bartering | 3 | 1/2 move | +20% |
  | Cobblestone Road | Construction | 4 | 1/3 move | +32% |
  | Paved Road | Industrialisation | 4 | 1/4 move | +45% |
  | Tarmac Road | Combustion | 5 | 1/6 move | +60% |

  Cities count as roads. Roads serve everyone - including invading armies. The AI links its cities with roads and upgrades them as it learns.
* **Nuclear weapons, again.** *Nuclear Fission* lets a city build a **Nuclear Warhead** (needs Uranium). Warheads wait in your silos; click the warhead count on the top bar, pick any tile within 12 of one of your cities and confirm. Everything next to the blast is destroyed, cities are wrecked and lose buildings, fallout spreads, your victims declare war and others who know you may join them. Hard and Expert rivals will use them too - rarely.
* **Achievements** - 43 of them, from *Spark of Genius* (discover Fire) to *Full Troop* (win with every primate), including the essential **Here We Go Again** for detonating a nuclear weapon. They belong to your profile rather than a saved game (`%APPDATA%\RebirthOfCivilisation\achievements.ini`), pop up as you earn them, and are listed under *Achievements* on the title screen and in the menu. Each has a stable API name (e.g. `HERE_WE_GO_AGAIN`) so a future Steam build can map them straight onto Steam achievements. Games where every civilisation is AI-controlled earn nothing.
* **Difficulty means a smarter AI, not a cheating one.** Unlike *Civilization*, no level gives the computer extra yields, units or settlers - every species plays by the same rules as you. Difficulty sets how well the rivals play:

  | Level | How the rivals play |
  |---|---|
  | Easy | Loosely: random research and traditions, fewer cities, slow to split and settle bands, timid and clumsy in war, never buy with gold |
  | Normal | Sensibly, but without long-term plans: cheapest-first research, random traditions, units fight wherever they meet the enemy |
  | Hard | With a plan: research valued by what it does for them (buildings, ruins they can dig, the road to Rebirth), traditions that suit their situation, more cities and Gatherers, walls when threatened, wars only when clearly stronger, armies that gather at a rally point with siege engines before attacking, attacks only at good odds, peace when losing |
  | Expert | As Hard, but sharper: bigger empires, near-noiseless decisions, opportunistic wars against rivals already fighting someone else, and a determined race to Rebirth |

  In AI-only test games where each species was given a different level, the levels finished in order - Expert ahead of Hard, Hard ahead of Normal, Normal ahead of Easy. Harder rivals also reach victory sooner, so the game is shorter and the race tighter.
* **Four ways to win:**
  * **Rebirth (science):** research *Rebirth* and complete the *Rebirth Project*.
  * **Domination:** control every original primate capital.
  * **Cultural:** adopt all 16 Traditions.
  * **Score:** have the highest score when time runs out (turn 500 at Standard speed).
* **A calendar that follows progress.** Years are counted *After the Fall* (AF), and how many pass each turn depends on how far the most advanced species has come:

  | Phase | Years per turn (Standard speed) |
  |---|---|
  | Before the Enlarged Neocortex | 20,000 |
  | Neocortex / Gestures / Proto-Speech | 10,000 / 6,000 / 3,000 |
  | Speech | 1,000 |
  | Science (from Advanced Speech): Awakening and Tribal / Ancient / Classical / Kingdoms | 50 / 40 / 25 / 20 |
  | Reclamation / Industrial / Modern / Rebirth | 10 / 5 / 2 / 1 |

  Evolution therefore unfolds over a hundred thousand years or so, while recorded history takes a few thousand, slowing to a year per turn at the end as in *Civilization*. Other speeds scale the years per turn so a game spans similar history.
* **Three game speeds**, chosen at setup as in *Civilization VI*:

  | Speed | Turn limit | Pace |
  |---|---|---|
  | Quick | 330 | Everything costs less; AI games usually run to around turns 280–330 |
  | **Standard** (default) | 500 | Like Civ VI Standard; AI games usually run to around turns 400–500 |
  | Epic | 750 | Everything costs much more; AI games usually run to around turns 700–750 |

  Speed scales the cost of science, production, growth, culture and evolution, along with timed effects (nuclear winter, fallout decay, feral escalation).
* Save and load, autosave, an event log, tooltips everywhere, a minimap, and four difficulty levels.

## The primates

| Primate | Leader | Ability | Unique unit |
|---|---|---|---|
| Chimpanzee | Alpha Kibo | **Tool Masters:** +15% Production, +10% combat | Hunting Party (Spear Troop) |
| Bonobo | Matriarch Lomako | **Make Love, Not War:** +25% Culture, +15% Food, −10% combat | Peacekeepers (Bow Troop) |
| Orangutan | Elder Tuan Batu | **Patient Minds:** +25% Science, forest and jungle cost only 1 move, −10% Food | Canopy Stalkers (Crossbows) |
| Gorilla | Silverback Mzee | **Silverback Strength:** +25% combat, faster healing, −10% Science | Silverback Guard (Warband) |
| Baboon | Warlord Kaba | **Savanna Troops:** +1 move for military units, military units cost 20% less | Baboon Raiders (Beast Riders) |
| Capuchin | Tinker Pico | **Clever Hands:** +75% ruin rewards, faster excavation, +20% Gold | Tinkerer (Gatherer) |
| Gibbon | Songmaster Wau | **Songs of the Canopy:** +20% Culture, +10% Science, borders grow 50% faster | Song Scout (Scout) |
| Japanese Macaque | Matriarch Yuki | **Snow Monkeys:** tundra and snow give +1 Food and +1 Production, half fallout damage, +10% Science | Onsen Guard (Iron Blades) |

![Choosing a primate](docs/screenshots/02_setup.png)

## Controls

| Input | Action |
|---|---|
| Left-click | Select a unit or city |
| Right-click | Move or attack with the selected unit (a path preview shows the number of turns) |
| Drag, or arrow keys | Scroll the map |
| Mouse wheel, or `+` / `-` | Zoom |
| `Enter` | End the turn |
| `Space` / `Tab` | Skip the unit / go to the next unit |
| `B` `F` `S` `E` `X` `M` | Settle / found city, fortify, sleep, excavate ruins, decontaminate, move mode |
| `G` / `R` | Split band / raise a war party from a band |
| `U` / `K` | Upgrade unit / board or leave a ship in a harbour |
| `T` `V` `P` `D` `L` | Technology tree / evolution / traditions / diplomacy / event log |
| `Ctrl+S` / `Ctrl+L` | Save / load |
| `Ctrl +` / `Ctrl -` / `Ctrl 0` | Interface size bigger / smaller / reset (also in the menu and on the title screen) |
| `F1` | How to play |
| `Esc` | Menu / close the current window |

## Screenshots

| | |
|---|---|
| ![Evolution tree](docs/screenshots/11b_evolution_mid.png) | ![Early tech tree](docs/screenshots/04c_tech_start.png) |
| ![A roaming band foraging](docs/screenshots/05_roaming_band.png) | ![The first settlement](docs/screenshots/07b_first_settlement.png) |
| ![Here We Go Again](docs/screenshots/18_here_we_go_again.png) | ![Achievements](docs/screenshots/17_achievements.png) |
| ![A fleet in harbour](docs/screenshots/19_naval.png) | ![Diplomacy](docs/screenshots/20_diplomacy.png) |
| ![Negotiating a deal](docs/screenshots/21_trade.png) | ![Combat effects](docs/screenshots/22_combat.png) |
| ![Caravans and trade routes](docs/screenshots/24_trade_routes.png) | ![Roads](docs/screenshots/25_roads.png) |
| ![Start](docs/screenshots/04_start_map.png) | ![The Fall](docs/screenshots/07_fall_popup.png) |
| ![A drought](docs/screenshots/11c_disaster.png) | ![Late game](docs/screenshots/16_lategame.png) |
| ![Excavating ruins](docs/screenshots/09_unit_panel.png) | ![Tech tree](docs/screenshots/10_tech_tree.png) |
| ![City screen](docs/screenshots/13_city_midgame.png) | ![Choosing a primate](docs/screenshots/02_setup.png) |

## Building from source

You need CMake 3.16 or newer and a C++17 compiler.

**Windows (Visual Studio 2019/2022):**

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
build\Release\RebirthOfCivilisation.exe
```

**Windows (MSYS2 / MinGW-w64):**

```sh
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Tests (any OS, including Linux and macOS):** the rules engine and UI are platform-independent, so the test tools build everywhere:

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build            # plays complete AI-vs-AI games and checks game-rule invariants
./build/rebirth_screens out/      # renders every screen to SVG (convert to PNG with rsvg-convert)
```

GitHub Actions builds the Windows `.exe` and runs the tests on every push. Each push to `main` publishes (or refreshes) the release for the version in `CMakeLists.txt`; bump that version to start a new release.

## Project layout

```
src/core/        Rules engine - no platform code
  Data.cpp         all game data: primates, techs, units, buildings, ruins, traditions
  Game.cpp         tiles, cities, units, movement, combat, research, ruin excavation
  Turn.cpp         turn processing, the nuclear war, fallout, ferals, diplomacy, victory
  MapGen.cpp       world generation, human settlements, start positions
  Bands.cpp        the nomadic age: roaming bands, foraging, settling
  Nukes.cpp        nuclear warheads: building, launching, the world's reaction
  Naval.cpp        naval AI (ship rules live in Game.cpp: moveCost, canEnter, canAttack)
  Transport.cpp    unit upgrades, ships carrying land units, AI ferrying
  Diplomacy.cpp    opinions, treaties, deals and AI diplomacy
  Trade.cpp        caravans, trade routes, roads
  Achievements.cpp achievement list, unlock checks and the profile file
  AI.cpp           computer opponents
  Save.cpp         save games
src/ui/          User interface, drawn through the small Canvas interface
  GameUI.cpp       screens, panels, popups, input
  UIMap.cpp        hex map rendering, unit motion and combat effects
  UIArt.cpp        primate portraits, the title scene, mushroom clouds
  UITrade.cpp      the caravan's route chooser
  UISound.cpp      which sounds, music and ambience play when
src/audio/Mixer.cpp  portable software mixer (effects, ambient loops, streamed Ogg music)
src/platform/win32/main_win32.cpp   Windows window, input and GDI+ renderer
src/platform/win32/audio_win32.cpp  plays the mixer through waveOut
assets/audio/    music, sound effects and ambience (CC BY-SA 3.0, from 0 A.D.)
third_party/     stb_vorbis Ogg decoder (public domain)
tools/fetch_audio.sh  rebuilds assets/audio from its sources
tests/           AI simulation tests, screenshot renderer, random-input stress test
res/             Icon and Windows resources
```

Most balance changes only need edits to `src/core/Data.cpp`.

## Contributing

Pull requests are welcome, from balance tweaks and new primates to better AI, sound, graphics and ports. Run `ctest` before you submit. Some ideas for future work:

* Proper unit artwork (sprites) instead of tokens
* Naval units and air units
* City-states made from surviving primate troops, and religion
* Multiplayer (hot-seat)
* A Linux and macOS front end (only `src/platform/` needs a new backend)

## Licence

The code is [MIT](LICENSE). The music and sounds in `assets/audio` are from [0 A.D.](https://play0ad.com/) by Wildfire Games and are licensed [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) (see [CREDITS](assets/audio/CREDITS.txt)); the stb_vorbis decoder is public domain. Contributors are expected to follow the [Code of conduct](CODE_OF_CONDUCT.md). Security issues: see [SECURITY.md](SECURITY.md).

## Code signing policy

Windows releases are built only by GitHub Actions from this repository. Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org). See the full [Code signing policy](CODE_SIGNING_POLICY.md), including team roles and the privacy policy.
