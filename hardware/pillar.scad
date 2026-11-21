// Pillar enclosures (print in PETG)
//
// Three printed parts sandwich the brine tank lid:
//
//          [ controller box ]   FireBeetle 2 ESP32-C6 + LP103665 LiPo, on top
//   =======[==== lid ====]======  8 mm cable hole + two 3.4 mm bolt holes
//            [ sensor hood ]    Pololu VL53L4CD carrier facing down, underneath
//
// Two M3 bolts go down through the box floor, the lid and the hood flange into
// nuts captured in the hood. The sensor wires go up through the centre hole.
// A fourth part, the spacer, sits on top of the sensor board inside the hood
// and clamps it against the hood's ledge when everything is bolted up.
//
// Parts (set `part`, or export with -D part="box" etc.):
//   box      - controller box. Print open side up. No supports.
//   box_lid  - lid for the box, 4 x M3 x 8 self-tapping screws. Print flat.
//   hood     - sensor hood. Print flange down (the face that touches the tank
//              lid goes on the bed). No supports.
//   spacer   - board clamp that sits inside the hood. Print flat.
//   template - drill template for the tank lid. Print flat, 1 mm thick.
//
// Measured / from datasheets:
//   - Pololu #3692 carrier: 13 x 18 x 2 mm (0.5" x 0.7" x 0.085")
//   - FireBeetle 2 ESP32-C6: 25.4 x 60 mm, USB-C on one short end
//   - LP103665 LiPo: 10 x 36 x 65 mm, plus a few mm for the protection board
// ASSUMED (check against your parts before printing):
//   - Pololu front face (sensor side) has no parts within ~1 mm of the edges.
//     The board rests on a 1 mm ledge around that face. If it rocks, shave the
//     ledge where it touches a part, or lower `ledge`.
//   - FireBeetle USB-C sits on the top side, centred on the short end.
//   - FireBeetle has nothing underneath within 2.5 mm of each short end (the
//     board rests on cross rails there; pin rows along the long edges stay
//     clear). Hold it down with a strip of double-sided foam tape on the rails.

/* [Part] */
part = "assembly"; // [assembly, box, box_lid, hood, spacer, template]
// Thickness of the tank lid, only used to draw the assembly
tank_lid_t = 4;
// Pull the parts apart in the assembly view (mm)
explode = 0;

/* [General] */
wall = 2.4;
floor_t = 2.4;
clear = 0.4;     // fit clearance around boards and battery
$fn = 48;

/* [Bolts] */
// Distance between the two M3 bolts (both parts and the template use it)
bolt_spacing = 34;
bolt_d = 3.4;          // M3 clearance
nut_af = 5.7;          // M3 nut across flats + clearance
nut_h = 2.6;
cable_d = 8;           // wire pass-through, matches the lid hole

/* [Sensor hood] */
pcb_w = 13;            // Pololu board, along hood X
pcb_l = 18;            // along hood Y
pcb_t = 2.2;           // board + parts on the back
ledge = 1.0;           // board rests on this lip (front face)
ledge_t = 1.2;         // thickness of the lip
hood_wall = 3.5;          // thick enough that the flared skirt keeps a 1.6 mm wall
wire_cavity = 8;       // room above the board for solder joints and wire bends
skirt = 5;             // recess below the board that shields the lens
skirt_flare = 30;      // deg from vertical; sensor FOV is 18 deg total
flange_t = 4;
flange_w = 26;         // flange size across the bolts' axis

/* [Controller box] */
batt_l = 70;           // 65 mm cell + protection board / leads
batt_w = 36;
batt_t = 10;
fb_l = 60;             // FireBeetle
fb_w = 25.4;
fb_rail_h = 3;         // board sits this high off the floor
pass_zone = 14;        // strip at one end with the cable hole and bolts
box_inner_h = 20;
usb_w = 12; usb_h = 7; // USB-C opening in the end wall
boss_d = 8;            // corner screw bosses (outside the walls)
pilot_d = 2.5;         // M3 self-tapping pilot
lid_t = 2.0;

// ---------------------------------------------------------------- derived
pocket_w = pcb_w + clear;
pocket_l = pcb_l + clear;
hood_ow = pocket_w + 2 * hood_wall;
hood_ol = pocket_l + 2 * hood_wall;
win_w = pocket_w - 2 * ledge;
win_l = pocket_l - 2 * ledge;
board_z = wire_cavity;                      // top (back) face of the board
ledge_z = board_z + pcb_t;                  // ledge top
hood_h = ledge_z + ledge_t + skirt;
flare = skirt * tan(skirt_flare);
flange_l = bolt_spacing + 14;

in_w = batt_w + clear + 2 + fb_w + clear + 1.6;   // battery bay + divider + board bay
in_l = pass_zone + max(batt_l, fb_l + 2) + clear;
out_w = in_w + 2 * wall;
out_l = in_l + 2 * wall;
out_h = floor_t + box_inner_h;
batt_y0 = 0;
fb_y0 = batt_w + clear + 2;                       // board bay starts after divider
fb_x1 = in_l;                                     // USB end against the +X wall
fb_x0 = fb_x1 - (fb_l + clear);

module hex(af, h) { cylinder(d = af / cos(30), h = h, $fn = 6); }

// ---------------------------------------------------------------- hood
// z = 0 is the face against the tank lid; +z points down toward the salt.
module hood() {
  difference() {
    union() {
      // flange
      translate([-flange_l / 2, -flange_w / 2, 0]) cube([flange_l, flange_w, flange_t]);
      // body
      translate([-hood_ow / 2, -hood_ol / 2, 0]) cube([hood_ow, hood_ol, hood_h]);
    }
    // wire cavity + board pocket (open toward the lid)
    translate([-pocket_w / 2, -pocket_l / 2, -1]) cube([pocket_w, pocket_l, ledge_z + 1]);
    // window through the ledge
    translate([-win_w / 2, -win_l / 2, ledge_z - 0.01]) cube([win_w, win_l, ledge_t + 0.02]);
    // flared skirt below the window
    translate([0, 0, ledge_z + ledge_t])
      hull() {
        translate([-win_w / 2, -win_l / 2, 0]) cube([win_w, win_l, 0.01]);
        translate([-win_w / 2 - flare, -win_l / 2 - flare, skirt]) cube([win_w + 2 * flare, win_l + 2 * flare, 0.02]);
      }
    // bolts and nut traps (nuts go in from the salt side of the flange)
    for (sx = [-1, 1]) translate([sx * bolt_spacing / 2, 0, -1]) {
      cylinder(d = bolt_d, h = flange_t + 2);
      translate([0, 0, 1 + flange_t - nut_h]) rotate([0, 0, 30]) hex(nut_af, nut_h + 1);
    }
  }
}

// Clamp that sits on the back of the board inside the wire cavity.
module spacer() {
  sw = pocket_w - clear;
  sl = pocket_l - clear;
  t = 1.6;
  difference() {
    translate([-sw / 2, -sl / 2, 0]) cube([sw, sl, wire_cavity]);
    translate([-sw / 2 + t, -sl / 2 + t, -1]) cube([sw - 2 * t, sl - 2 * t, wire_cavity + 2]);
  }
}

// ---------------------------------------------------------------- box
// Box coordinates: inner floor corner at (0, 0, floor_t), walls outside.
module box() {
  difference() {
    union() {
      translate([-wall, -wall, 0]) cube([out_l, out_w, out_h]);
      for (p = box_boss_positions()) translate([p[0], p[1], 0]) cylinder(d = boss_d, h = out_h);
    }
    // cavity
    translate([0, 0, floor_t]) cube([in_l, in_w, box_inner_h + 1]);
    // pass-through: cable hole and two countersunk bolt holes in the floor
    translate([pass_zone / 2, in_w / 2, -1]) {
      cylinder(d = cable_d, h = floor_t + 2);
      for (sy = [-1, 1]) translate([0, sy * bolt_spacing / 2, 0]) {
        cylinder(d = bolt_d, h = floor_t + 2);
        translate([0, 0, 1 + floor_t - 1.7]) cylinder(d1 = bolt_d, d2 = 6.4, h = 1.71);
      }
    }
    // USB-C opening in the +X end wall, centred on the board
    usb_z = floor_t + fb_rail_h + 1.6 + 1.6;
    translate([in_l - 1, fb_y0 + (fb_w + clear) / 2 - usb_w / 2, usb_z - usb_h / 2]) cube([wall + 2, usb_w, usb_h]);
    // screw pilots in the bosses
    for (p = box_boss_positions()) translate([p[0], p[1], out_h - 12]) cylinder(d = pilot_d, h = 13);
  }
  // (extras overlap the floor/walls by e so they fuse into one solid)
  e = 0.05;
  // divider between battery bay and board bay (low, wires pass over it)
  translate([pass_zone, batt_w + clear, floor_t - e]) cube([in_l - pass_zone + e, 2, 6 + e]);
  // wall between the pass-through strip and the battery bay, low
  translate([pass_zone - 1.6, -e, floor_t - e]) cube([1.6, batt_w + clear + 2 * e, 4 + e]);
  // board cross rails at each short end of the board
  for (x = [fb_x0, fb_x1 - 2.5]) translate([x, fb_y0, floor_t - e]) cube([2.5 + (x > fb_x0 ? e : 0), fb_w + clear, fb_rail_h + e]);
}

function box_boss_positions() = [
  [-wall, -wall], [in_l + wall, -wall], [-wall, in_w + wall], [in_l + wall, in_w + wall]
];

module box_lid() {
  difference() {
    union() {
      hull() for (p = box_boss_positions()) translate([p[0], p[1], 0]) cylinder(d = boss_d, h = lid_t);
      // locating lip inside the walls
      translate([clear / 2, clear / 2, lid_t]) difference() {
        cube([in_l - clear, in_w - clear, 2]);
        translate([1.6, 1.6, -1]) cube([in_l - clear - 3.2, in_w - clear - 3.2, 4]);
      }
    }
    for (p = box_boss_positions()) translate([p[0], p[1], -1]) cylinder(d = bolt_d, h = lid_t + 2);
  }
}

// ---------------------------------------------------------------- template
module template() {
  difference() {
    translate([-flange_l / 2, -flange_w / 2, 0]) cube([flange_l, flange_w, 1]);
    translate([0, 0, -1]) cylinder(d = 2, h = 3);               // centre punch mark
    for (sx = [-1, 1]) translate([sx * bolt_spacing / 2, 0, -1]) cylinder(d = 2, h = 3);
  }
}

// ---------------------------------------------------------------- assembly
module assembly() {
  // tank lid slab
  color("lightgray", 0.6) difference() {
    translate([-60, -45, 0]) cube([120, 90, tank_lid_t]);
    translate([0, 0, -1]) cylinder(d = cable_d, h = tank_lid_t + 2);
    for (sx = [-1, 1]) translate([sx * bolt_spacing / 2, 0, -1]) cylinder(d = bolt_d, h = tank_lid_t + 2);
  }
  // under the lid, top to bottom: spacer, sensor board (face down), hood.
  // (mirrored so +z in the part files points down toward the salt)
  color("gold") translate([0, 0, -explode / 3]) mirror([0, 0, 1]) spacer();
  color("purple") translate([0, 0, -explode * 2 / 3]) mirror([0, 0, 1])
    translate([-pcb_w / 2, -pcb_l / 2, board_z]) cube([pcb_w, pcb_l, pcb_t]);
  color("orange") translate([0, 0, -explode]) mirror([0, 0, 1]) hood();
  // box on top: pass-through hole over the lid hole, bolt axis along X
  translate([0, 0, tank_lid_t + explode]) rotate([0, 0, 90]) translate([-pass_zone / 2, -in_w / 2, 0]) {
    color("steelblue") box();
    // box lid, flipped onto the box
    color("lightsteelblue") translate([0, in_w, out_h + lid_t + explode * 0.6]) mirror([0, 1, 0]) mirror([0, 0, 1]) box_lid();
  }
}

if (part == "hood") hood();
else if (part == "spacer") spacer();
else if (part == "box") box();
else if (part == "box_lid") box_lid();
else if (part == "template") template();
else assembly();
