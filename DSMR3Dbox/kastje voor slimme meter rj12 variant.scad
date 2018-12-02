xmax=40.7;
ymax=66;
zmax=23.5;
hoogte_rj12=15;
breedte_rj12=13.3;
//bodem_gat_x=38; 
//afstand tussen gaten voor de pcb bevestiging
bodem_gat_y=39.4;
bodem_gat_x=28.5;
diepte_gat=15; //schroefgat in hoekpunten
gat_top=5;  // afstand vanaf hoekpunt voor bovenplaat
// plaatsing halverwege tussen zijplaten
// het printed circuit moet precies tegen de wand komen
// aan de kant waar de RJ12 connector zit
// gat_bodem is de hartafstand van het gat in de printplaat
// tot de buitenkant (die op positie xmax zit
gat_bodem_x=5.6; 
// hart afstand van de gaten in het printed circuit
// in de x richting (kleinste afstand)
hart_afstand_x=28.5;
// hart afstand y richting van het pcb
hart_afstand_y=39.0;
gat_bodem_y=(ymax-bodem_gat_y)/2;
// dikte hoeksupport
hoek_support=8;
dikte=2.3;
dikte_pcb=1.7;
pcb_support=zmax-dikte_pcb-hoogte_rj12-0.5;
verlaging_hoek_support=dikte;
$fn=25;
//minkowski()

difference()
{union()
    // onderplaat
{cube([xmax,ymax,dikte]);
    // plaat voorkant
cube([dikte,ymax,zmax]);
   
       // plaat zijkant links
cube([xmax,dikte,zmax]);
    // plaat rechterzijkant
translate([xmax-dikte,0,0])
cube([dikte,ymax,zmax]);
     // ondersteuning rj12 female connector
    translate([xmax-14,ymax-gat_bodem_y-breedte_rj12-1,0])
    cube([8,17,zmax-hoogte_rj12-dikte_pcb-0.3]);
    // plaat achterkant
translate([0,ymax-dikte,0])
cube([xmax,dikte,zmax]);
    //hoekrondjes
translate([1.5,1.5,0])
cylinder(r=2,h=zmax);
translate([1.5,ymax-1.5,0])
cylinder(r=2,h=zmax);
translate([xmax-1.5,1.5,0])
cylinder(r=2,h=zmax);
translate([xmax-1.5,ymax-1.5,0])
cylinder(r=2,h=zmax);
    
    // stuk voor muurbevestiging
    translate([xmax/2-5,-10,0])
    cube([10,10,3]);
    translate([xmax/2-5,ymax,0])
    cube([10,10,3]);
 
// 4 cylinders voor bevestiging bovenplaat
translate([hoek_support/1.8,hoek_support/1.8,0]) 
cylinder(r=4.5,h=zmax);
translate([xmax-hoek_support/1.8,hoek_support/1.8,0])
cylinder(r=4.5,h=zmax);
translate([xmax-hoek_support/1.8,ymax-hoek_support/1.8,0])
cylinder(r=4.5,h=zmax);
translate([hoek_support/1.8,ymax-hoek_support/1.8,0])
cylinder(r=4.5,h=zmax);
// de drie ronde supports voor het pcb
translate([xmax-gat_bodem_x,gat_bodem_y,0]) 
cylinder(d=6,h=pcb_support);
translate([xmax-gat_bodem_x-hart_afstand_x,ymax-gat_bodem_y,0]) 
cylinder(d=6,h=pcb_support);
translate([xmax-gat_bodem_x-hart_afstand_x,gat_bodem_y,0]) 
cylinder(d=6,h=pcb_support);}
// dit zijn de stukken die eruit gaan
//hieronder het stuk uit de rechterzijwand
translate([xmax-dikte-1,ymax-gat_bodem_y-breedte_rj12+0.6,zmax-hoogte_rj12])
cube([4,breedte_rj12+0.2,hoogte_rj12]);
// de gaten in de muurbevestiging (zijpoootjes)
translate([xmax/2,-5,-1])
    cylinder(d=3,h=5);
    translate([xmax/2,ymax+5,-1])
    cylinder(d=3,h=5);
//gat in printed circuit board bevestiging
translate([xmax-gat_bodem_x,gat_bodem_y,dikte]) 
cylinder(d=3,h=7);
translate([xmax-gat_bodem_x-hart_afstand_x,ymax-gat_bodem_y,dikte]) 
cylinder(d=3,h=7);
translate([xmax-gat_bodem_x-hart_afstand_x,gat_bodem_y,dikte]) 
cylinder(d=3,h=7);
// gaten voor de bevestiging van de bovenplaat
translate([gat_top,gat_top,zmax-diepte_gat])
cylinder(d=3,h=diepte_gat+dikte+1);
translate([xmax-gat_top,gat_top,zmax-dikte-diepte_gat])
cylinder(d=3,h=diepte_gat+dikte+1);
translate([gat_top,ymax-gat_top,zmax-dikte-diepte_gat])
cylinder(d=3,h=diepte_gat+dikte+1);
translate([xmax-gat_top,ymax-gat_top,zmax-dikte-diepte_gat])
cylinder(d=3,h=diepte_gat+dikte+1);
// gaten voor uitstekende bevestigingsplug rj12 connector
translate([xmax-10.5,ymax-gat_bodem_y-10.5,dikte])
cylinder(d=4,h=10);
translate([xmax-10.5,ymax-gat_bodem_y-0.9,dikte])

cylinder(d=4,h=10);}