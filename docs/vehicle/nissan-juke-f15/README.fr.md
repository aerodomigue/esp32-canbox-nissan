# Manuels d’atelier Nissan Juke F15

[English](README.md)

## Édition et applicabilité

Cette archive contient **57 PDF de manuels Nissan** (78.5 Mio de contenu PDF).
[FWD.pdf](FWD.pdf#page=2) identifie le manuel comme celui du **Nissan Juke 2012**.
Les chapitres utiles au projet vérifiés portent **Revision: 2011 October / 2012 JUKE**.
L’avant-propos et les informations d’identification situent cette collection dans le contexte **nord-américain**.
Une publication en 2011 ne constitue pas un manuel du millésime 2011.

Le véhicule du projet est un **F15 phase 1 de 2011, 1.6 DIG-T MR16DDT, boîte manuelle**.
Ces documents apportent des informations sur cette famille de véhicules ; les différences de marché, de millésime et d’équipement restent à vérifier.
LAN-27 distingue des variantes MR16DDT M/T ou CVT, 2WD ou AWD, avec ou sans Integrated Control System.
Les fonctions TCM, AWD et climatisation/afficheur ne sont donc pas présentes sur toutes les variantes.

## Ce que ces manuels établissent

- **LAN-24–31 :** principes de câblage CAN, variantes d’équipement et matrices d’émission/réception des signaux.
  Les noms de signaux et de calculateurs n’indiquent pas les identifiants d’arbitrage numériques, positions d’octets ou masques de bits.
- **MWI-6 et MWI-15–16 :** le combiné reçoit les signaux moteur et de vitesse ;
  les capteurs de niveau de carburant et de température extérieure ont aussi des entrées câblées directement.
  Le combiné calcule les consommations instantanée/moyenne et l’autonomie restante.
- **MWI-20–22, BRC-38–40 et sections de diagnostic EXL :** les noms des moniteurs CONSULT indiquent des valeurs observables.
  Ils ne donnent pas l’adresse CAN, le service, le PID/DID et le format de réponse nécessaires à une lecture indépendante.
- **FL-31 :** cette édition indique environ 50,0 L en 2WD et 45,0 L en AWD.
  Cela ne valide pas une calibration de 45 L pour le véhicule manuel de 2011 du projet.

Les manuels archivés **ne valident pas les IDs CAN du preset actuel, le diviseur RPM, l’offset de direction ou l’échelle de carburant**.
Une capture CAN et des observations sur le véhicule cible restent nécessaires pour confirmer un mapping.
Le CAN Nissan du véhicule et le [protocole UART Raise Toyota](../../protocols/raise-toyota-rav4/) destiné à l’autoradio sont deux protocoles distincts.
[L’analyse du mapping CAN](../../JUKE_F15_CAN_MAPPING.fr.md) présente les preuves actuelles et leurs limites.

## Index des fichiers

Les intitulés sont traduits depuis le **Quick Reference Index de FWD.pdf**.
Les noms de fichiers sont conservés, notamment `acc.pdf` en minuscules.
MWI désigne le combiné, FL le carburant, EXL l’éclairage extérieur,
GW les vitrages et PCS la commande d’alimentation.

| Fichier | Section |
| --- | --- |
| [FWD.pdf](FWD.pdf) | Avant-propos et index général |
| [GI.pdf](GI.pdf) | Informations générales |
| [acc.pdf](acc.pdf) | Commande d’accélérateur |
| [EM.pdf](EM.pdf) | Mécanique moteur |
| [EC.pdf](EC.pdf) | Gestion moteur |
| [LU.pdf](LU.pdf) | Lubrification moteur |
| [CO.pdf](CO.pdf) | Refroidissement moteur |
| [FL.pdf](FL.pdf) | Circuit de carburant |
| [EX.pdf](EX.pdf) | Échappement |
| [STR.pdf](STR.pdf) | Démarrage |
| [CL.pdf](CL.pdf) | Embrayage |
| [TM.pdf](TM.pdf) | Boîtes de vitesses et transmission |
| [DLN.pdf](DLN.pdf) | Chaîne cinématique |
| [FAX.pdf](FAX.pdf) | Essieu avant |
| [RAX.pdf](RAX.pdf) | Essieu arrière |
| [FSU.pdf](FSU.pdf) | Suspension avant |
| [RSU.pdf](RSU.pdf) | Suspension arrière |
| [WT.pdf](WT.pdf) | Roues et pneumatiques |
| [BR.pdf](BR.pdf) | Freinage |
| [PB.pdf](PB.pdf) | Frein de stationnement |
| [BRC.pdf](BRC.pdf) | Commande de freinage, ABS et VDC |
| [ST.pdf](ST.pdf) | Direction |
| [STC.pdf](STC.pdf) | Commande de direction assistée |
| [SB.pdf](SB.pdf) | Ceintures de sécurité |
| [SR.pdf](SR.pdf) | Airbags SRS |
| [SRC.pdf](SRC.pdf) | Commande des airbags SRS |
| [VTL.pdf](VTL.pdf) | Ventilation |
| [HA.pdf](HA.pdf) | Chauffage et climatisation |
| [HAC.pdf](HAC.pdf) | Commande du chauffage et de la climatisation |
| [INT.pdf](INT.pdf) | Habitacle |
| [IP.pdf](IP.pdf) | Planche de bord |
| [SE.pdf](SE.pdf) | Sièges |
| [DLK.pdf](DLK.pdf) | Portes et serrures |
| [SEC.pdf](SEC.pdf) | Antidémarrage et sécurité d’accès |
| [GW.pdf](GW.pdf) | Vitrages et vitres |
| [PWC.pdf](PWC.pdf) | Commande des vitres électriques |
| [RF.pdf](RF.pdf) | Toit |
| [EXT.pdf](EXT.pdf) | Éléments extérieurs |
| [BRM.pdf](BRM.pdf) | Réparation de carrosserie |
| [MIR.pdf](MIR.pdf) | Rétroviseurs |
| [EXL.pdf](EXL.pdf) | Éclairage extérieur |
| [INL.pdf](INL.pdf) | Éclairage intérieur |
| [WW.pdf](WW.pdf) | Essuie-glaces et lave-glaces |
| [DEF.pdf](DEF.pdf) | Désembuage |
| [HRN.pdf](HRN.pdf) | Avertisseur sonore |
| [PWO.pdf](PWO.pdf) | Prises électriques |
| [BCS.pdf](BCS.pdf) | Commande de carrosserie et BCM |
| [LAN.pdf](LAN.pdf) | Réseau de communication, dont CAN |
| [PCS.pdf](PCS.pdf) | Commande d’alimentation, dont IPDM E/R |
| [CHG.pdf](CHG.pdf) | Circuit de charge |
| [PG.pdf](PG.pdf) | Alimentation, masses et composants de circuit |
| [MWI.pdf](MWI.pdf) | Combiné, témoins et indicateurs |
| [WCS.pdf](WCS.pdf) | Avertisseurs sonores du véhicule |
| [AV.pdf](AV.pdf) | Audio, vidéo et navigation |
| [CCS.pdf](CCS.pdf) | Régulation de vitesse |
| [DMS.pdf](DMS.pdf) | Sélection des modes de conduite |
| [MA.pdf](MA.pdf) | Entretien |

## Source et Git LFS

Le dépôt archive des documents Nissan récupérés via
[NICOclub Nissan Service Manuals](https://www.nicoclub.com/nissan-service-manuals).
L’édition, les titres et les pages imprimées font foi ; le nom du dossier ne prouve pas leur applicabilité à tous les millésimes F15.

Les PDF utilisent **Git LFS**. Un fichier contenant `version https://git-lfs.github.com/spec/v1`
est un pointeur, pas un PDF lisible. Pour récupérer les contenus LFS si nécessaire :

```sh
git lfs pull --include="docs/vehicle/nissan-juke-f15/*.pdf"
```

Les documents conservent leurs mentions de copyright Nissan.
