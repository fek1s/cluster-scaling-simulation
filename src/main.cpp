#include <iostream>
#include <cmath>
#include <vector>
#include <limits>
#include "simlib.h"

using namespace std;

/* PARAMETRY SIMULACE */
const double SIMULATION_TIME = 24 * 3600; // Simulujeme 24 hodin (v sekundách)
const double SERVICE_TIME = 0.1;          // Základní čas zpracování jednoho požadavku (v sekundách)
const int MAX_CONTAINERS = 40;
const int MIN_CONTAINERS = 1;
const double SCALING_INTERVAL = 120;      // Interval kontroly pro škálování (v sekundách)
const double SLA_RESPONSE_TIME = 0.2;     // SLA: 95% požadavků musí být obslouženo do 200 ms
const double COST_PER_CONTAINER = 0.1;    // Náklady na jeden kontejner za hodinu
const double ALPHA = 0.02;                // Koeficient ovlivňující nárůst latence
const double SCALE_UP_LOAD = 38;           // Prahová hodnota pro škálování nahoru (průměrná zátěž)
const double SCALE_DOWN_LOAD = 10;         // Prahová hodnota pro škálování dolů (průměrná zátěž)
const double CONTAINER_STARTUP_TIME = 0; // Doba spuštění kontejneru v sekundách
const int REQUESTS_MULTIPLIER = 25;        // Násobitel počtu požadavků

/* GLOBÁLNÍ PROMĚNNÉ */
int total_containers = 0;
int max_containers_created = 0;
int sla_violations = 0;
long total_requests = 0;
double operating_cost = 0.0;

/* DATASET ZÁTĚŽE */
int requests_per_minute[1440] = {
    907, 922, 1111, 1101, 1098, 925, 1018, 1186, 1092, 1170,
    1241, 1279, 1216, 1096, 892, 1019, 1127, 950, 994, 986,
    961, 1076, 1078, 900, 1112, 1076, 1181, 1026, 1022, 1001,
    1101, 1017, 1111, 1102, 1001, 1033, 950, 957, 895, 1117,
    1150, 1220, 1156, 1090, 1150, 1227, 1056, 991, 1129, 1090,
    1045, 1051, 839, 811, 876, 697, 958, 933, 960, 951,
    948, 1033, 884, 871, 912, 993, 870, 870, 907, 962,
    1060, 1010, 999, 1033, 845, 906, 1066, 803, 887, 970,
    932, 830, 1049, 971, 938, 814, 916, 915, 812, 759,
    744, 750, 774, 870, 935, 875, 885, 849, 791, 789,
    678, 701, 860, 699, 749, 810, 845, 771, 979, 830,
    1070, 852, 728, 663, 807, 700, 812, 801, 796, 800,
    874, 1109, 847, 813, 838, 734, 823, 785, 828, 847,
    777, 832, 696, 705, 696, 833, 743, 821, 823, 762,
    865, 852, 918, 986, 820, 983, 879, 824, 894, 871,
    751, 869, 938, 864, 828, 861, 787, 813, 692, 784,
    916, 859, 792, 769, 830, 742, 770, 777, 681, 863,
    793, 794, 847, 866, 958, 661, 574, 680, 714, 578,
    623, 735, 568, 645, 627, 701, 626, 551, 486, 664,
    590, 600, 708, 661, 600, 616, 713, 870, 737, 687,
    811, 767, 701, 644, 613, 622, 571, 565, 678, 634,
    492, 672, 726, 661, 637, 657, 491, 602, 665, 597,
    665, 566, 634, 740, 595, 595, 717, 682, 579, 660,
    719, 684, 578, 605, 650, 658, 602, 457, 546, 599,
    632, 550, 439, 501, 582, 615, 511, 722, 653, 687,
    624, 549, 487, 448, 537, 548, 423, 516, 734, 569,
    449, 578, 502, 499, 559, 526, 592, 562, 578, 704,
    460, 619, 522, 440, 524, 486, 479, 517, 588, 617,
    483, 469, 476, 458, 469, 523, 577, 561, 488, 546,
    544, 526, 594, 483, 554, 529, 557, 631, 544, 502,
    451, 437, 443, 654, 542, 541, 578, 459, 478, 493,
    409, 411, 430, 444, 605, 490, 450, 474, 651, 579,
    609, 437, 483, 504, 550, 614, 665, 493, 569, 486,
    608, 491, 476, 531, 420, 461, 393, 373, 536, 537,
    467, 463, 552, 637, 598, 495, 551, 480, 554, 527,
    487, 492, 519, 421, 530, 496, 525, 432, 468, 342,
    547, 604, 627, 441, 493, 435, 348, 383, 346, 406,
    401, 390, 449, 506, 492, 491, 505, 526, 431, 511,
    504, 569, 438, 468, 428, 517, 432, 535, 477, 593,
    504, 452, 442, 477, 462, 379, 446, 503, 490, 592,
    659, 499, 489, 457, 677, 687, 733, 577, 496, 568,
    619, 538, 551, 663, 529, 589, 535, 659, 488, 420,
    595, 546, 673, 569, 629, 576, 499, 506, 575, 567,
    565, 612, 530, 502, 453, 471, 572, 581, 445, 593,
    466, 568, 508, 614, 645, 711, 547, 715, 874, 798,
    712, 764, 724, 630, 624, 705, 662, 648, 848, 821,
    929, 710, 757, 689, 775, 714, 715, 863, 686, 593,
    580, 614, 710, 827, 779, 708, 795, 795, 672, 723,
    865, 732, 865, 724, 533, 643, 657, 709, 825, 942,
    833, 761, 910, 859, 873, 726, 813, 807, 955, 872,
    679, 694, 850, 838, 725, 759, 1087, 834, 832, 934,
    916, 816, 836, 851, 806, 936, 843, 842, 859, 878,
    946, 844, 964, 879, 980, 1023, 1008, 784, 885, 944,
    1071, 1031, 998, 970, 832, 736, 734, 985, 1273, 1100,
    856, 877, 977, 984, 1024, 946, 902, 1022, 1079, 1026,
    1148, 782, 1062, 1118, 1300, 1022, 1005, 880, 953, 1114,
    1012, 1000, 984, 1160, 962, 1078, 1028, 1126, 1149, 1070,
    1081, 1061, 1171, 1245, 1221, 1349, 1318, 1286, 1135, 1149,
    1141, 1152, 1054, 1136, 1181, 1212, 1292, 1007, 920, 945,
    1177, 1108, 1300, 1108, 1027, 1015, 907, 1140, 1089, 1170,
    1089, 1206, 1093, 1020, 1239, 1130, 1001, 1245, 983, 1087,
    1261, 1362, 1148, 1109, 1017, 1389, 1333, 1376, 1242, 1264,
    1293, 1202, 1397, 1403, 1252, 1369, 1405, 1301, 1212, 1277,
    1251, 1169, 1551, 1311, 1448, 1132, 1340, 1322, 1086, 1295,
    1375, 1464, 1492, 1442, 1274, 1423, 1427, 1385, 1365, 1526,
    1489, 1533, 1323, 1414, 1489, 1430, 1408, 1401, 1192, 1290,
    1231, 1507, 1554, 1416, 1217, 1170, 1270, 1303, 1434, 1258,
    1296, 1149, 1507, 1505, 1378, 1485, 1308, 1579, 1488, 1422,
    1507, 1348, 1432, 1492, 1531, 1412, 1390, 1584, 1462, 1401,
    1397, 1421, 1349, 1483, 1441, 1522, 1492, 1462, 1434, 1362,
    1530, 1473, 1661, 1426, 1201, 1404, 1345, 1489, 1338, 1479,
    1464, 1527, 1460, 1522, 1494, 1539, 1610, 1372, 1348, 1516,
    1105, 1338, 1444, 1320, 1425, 1396, 1137, 1385, 1500, 1540,
    1574, 1714, 1651, 1471, 1509, 1480, 1868, 1613, 1459, 1428,
    1530, 1436, 1472, 1417, 1639, 1273, 1534, 1427, 1587, 1520,
    1352, 1208, 1682, 1762, 1549, 1648, 1579, 1711, 1677, 1708,
    1539, 1431, 1465, 1761, 1505, 1781, 1557, 1482, 1707, 1652,
    1657, 1631, 1725, 1901, 1792, 1534, 1520, 1714, 1480, 1421,
    1631, 1291, 1523, 1668, 1696, 1552, 1443, 1496, 1774, 1518,
    1728, 1633, 1839, 1407, 1494, 1620, 1767, 1704, 1384, 1544,
    1656, 1762, 1690, 1646, 1451, 1602, 1705, 1731, 1583, 1602,
    1696, 1469, 1420, 1511, 1341, 1496, 1377, 1494, 1534, 1582,
    1478, 1596, 1550, 1392, 1426, 1415, 1506, 1591, 1582, 1464,
    1556, 1637, 1810, 1711, 1679, 1866, 1711, 1690, 1667, 1619,
    1900, 1755, 1796, 1986, 1783, 1585, 1313, 1352, 1419, 1447,
    1739, 1462, 1475, 1755, 1693, 1595, 1730, 1945, 1681, 1650,
    1879, 1759, 1690, 1756, 1675, 1813, 1938, 1751, 1854, 1600,
    1699, 1683, 1685, 1675, 1756, 1524, 1771, 1655, 1502, 1682,
    1713, 1662, 1916, 1776, 1618, 1671, 1672, 1671, 1668, 1675,
    1598, 1542, 1490, 1692, 1553, 1196, 1492, 1590, 1744, 1622,
    1593, 1710, 1857, 1527, 1748, 1806, 1637, 1790, 1801, 1915,
    1821, 1650, 1722, 1392, 1631, 1628, 1751, 1775, 1595, 1610,
    1673, 1616, 1716, 1387, 1736, 1885, 1900, 1970, 1974, 1746,
    1598, 1773, 1729, 1604, 1697, 1690, 1888, 1836, 1857, 1913,
    2184, 1788, 1686, 1483, 1703, 1740, 1715, 1499, 1617, 1861,
    1743, 1651, 1667, 1782, 1676, 1818, 1843, 1645, 1716, 1976,
    1641, 1543, 1495, 1722, 1849, 1777, 1688, 1585, 1481, 1724,
    1776, 1730, 1931, 1963, 1997, 1845, 1876, 1906, 1720, 1722,
    1794, 2192, 1891, 1708, 1777, 1742, 1668, 1742, 1578, 1655,
    1575, 1782, 1805, 1785, 1606, 1604, 1688, 1472, 1696, 1616,
    1479, 1682, 1675, 1563, 1454, 1735, 1723, 1768, 1942, 2029,
    1739, 1670, 1325, 1471, 1582, 1721, 1716, 1750, 1765, 1605,
    1674, 1482, 1502, 1570, 1735, 1827, 1536, 1451, 1789, 1407,
    1497, 1635, 1693, 1408, 1528, 1675, 1826, 1652, 1651, 1781,
    1740, 1620, 1732, 1664, 1664, 1671, 1789, 1529, 1339, 1600,
    1576, 1693, 1581, 1786, 1605, 1496, 1355, 1467, 1609, 1604,
    1626, 1821, 1497, 1577, 1535, 1435, 1430, 1536, 1360, 1564,
    1568, 1586, 1512, 1566, 1633, 1623, 1513, 1391, 1563, 1612,
    1763, 1897, 1671, 1326, 1252, 1572, 1592, 1475, 1483, 1530,
    1481, 1305, 1505, 1332, 1537, 1811, 1385, 1412, 1498, 1260,
    1386, 1562, 1502, 1534, 1522, 1555, 1622, 1471, 1452, 1425,
    1371, 1384, 1471, 1388, 1268, 1604, 1515, 1491, 1418, 1302,
    1419, 1592, 1701, 1754, 1437, 1679, 1405, 1428, 1392, 1403,
    1471, 1346, 1378, 1509, 1596, 1324, 1387, 1273, 1541, 1386,
    1543, 1460, 1365, 1265, 1414, 1523, 1413, 1373, 1224, 1324,
    1230, 1437, 1405, 1517, 1512, 1290, 1389, 1308, 1522, 1529,
    1287, 1473, 1436, 1403, 1509, 1216, 1400, 1427, 1348, 1353,
    1326, 1407, 1314, 1353, 1220, 1383, 1410, 1518, 1235, 1382,
    1179, 1162, 1200, 1169, 1200, 1283, 1255, 1321, 1262, 1216,
    1294, 1322, 1168, 1130, 1110, 1168, 1402, 1413, 1252, 1311,
    1239, 1088, 1059, 1104, 1101, 1274, 1264, 1194, 966, 1165,
    1360, 1235, 1148, 1145, 1490, 1446, 1468, 1385, 1285, 1191,
    1197, 1096, 1338, 1098, 1455, 1124, 1166, 1320, 1026, 1113,
    1386, 1252, 1414, 1291, 1115, 1261, 1409, 1028, 1156, 1254,
    1292, 1261, 1091, 1017, 1175, 1292, 934, 951, 1063, 1080,
    1153, 1263, 1201, 1339, 1241, 1199, 1141, 1172, 1190, 1003,
    1040, 1172, 1222, 1311, 1028, 1043, 1130, 1204, 1222, 1279,
    1138, 1245, 1176, 1159, 1176, 1270, 1340, 1133, 1201, 1124,
    1212, 1179, 1123, 1327, 1355, 1265, 1343, 1231, 1265, 1205,
    1195, 1177, 1123, 1124, 1262, 1378, 1236, 1248, 994, 1015,
    1085, 1159, 1025, 1293, 1165, 1251, 1345, 1055, 1115, 1133,
    1125, 1396, 1144, 1095, 1239, 1209, 1141, 1129, 1260, 1178,
    1152, 1189, 1172, 1245, 1243, 1170, 1290, 1283, 1199, 1209,
    1128, 1194, 1231, 1271, 1240, 1364, 1229, 1351, 1190, 1269,
    1134, 1227, 1184, 990, 993, 1125, 1145, 1155, 1063, 1048,
    1045, 1105, 1335, 1280, 1153, 1073, 1374, 1113, 979, 1002,
    1262, 1270, 1197, 1327, 1297, 1189, 1220, 1153, 1062, 1061,
    1083, 1189, 1145, 1210, 1228, 1115, 1225, 1240, 1153, 1201,
    1115, 1129, 1153, 1129, 1063, 1026, 1060, 1181, 1242, 1143,
    1098, 1021, 1309, 1219, 1141, 1010, 1081, 1085, 1207, 1197,
    1116, 1368, 1103, 1267, 1189, 1022, 1103, 1258, 1326, 1095,
    1000, 1087, 1089, 1182, 1323, 1427, 1156, 1147, 1165, 1212,
    911, 1159, 1206, 1126, 1085, 1148, 1292, 1154, 1182, 1086,
    1052, 917, 1006, 1028, 1001, 1067, 958, 1157, 1184, 1037,
};

/* PŘEDIKTIVNÍ ŠKÁLOVÁNÍ */
vector<int> predicted_load(1440, 0);

/* STATISTIKY */
Stat response_time_stat("Doba odezvy");
Histogram response_time_hist("Histogram doby odezvy", 0, 0.05, 20);

/* Před deklarací třídy Container deklarujeme pole containers */
class Container; // Předběžná deklarace třídy
Container* containers[MAX_CONTAINERS];    // Pole kontejnerů

/* TŘÍDA KONTEJNERU */
class Container {
public:
    int id;                  // Identifikátor kontejneru
    int active_requests;     // Počet aktivních požadavků
    double load;             // Zátěž kontejneru (počet aktivních požadavků)
    Stat* load_stat;         // Statistika zátěže
    double activation_time;   // Čas aktivace kontejneru
    double total_active_time; // Celkový čas, po který byl kontejner aktivní
    bool is_active;           // Indikátor, zda je kontejner aktivní
    bool is_ready;            // Indikátor, zda je kontejner připraven přijímat požadavky

    Container(int id) : id(id), active_requests(0), load(0.0), total_active_time(0.0), is_active(false), is_ready(false) {
        activation_time = Time;
        load_stat = new Stat();
    }

    // Přijme požadavek
    void AcceptRequest() {
        active_requests++;
        UpdateLoad();
    }

    // Uvolní požadavek po zpracování
    void ReleaseRequest() {
        active_requests--;
        UpdateLoad();
    }

    // Aktualizuje zátěž kontejneru
    void UpdateLoad() {
        load = active_requests;
        (*load_stat)(load);
    }

    // Deaktivuje kontejner
    void Deactivate() {
        if (is_active) {
            is_active = false;
            total_active_time += Time - activation_time;
        }
    }

    // Aktivuje kontejner
    void Activate() {
        if (!is_active) {
            is_active = true; // Kontejner bude aktivní po době spuštění
            is_ready = false;
            activation_time = Time;
        }
    }


    // Spustí kontejner po době spuštění
    void Start() {
        is_active = true;
        is_ready = true;
        activation_time = Time;
    }
};

/* PROCES SPUŠTĚNÍ KONTEJNERU */
class ContainerStartup : public Event {
public:
    Container* container;

    ContainerStartup(Container* c) : container(c) {}

    void Behavior() {
        container->Start();
        cout << "Kontejner " << container->id << " je připraven v čase " << Time << endl;
    }
};

/* TŘÍDA POŽADAVKU */
class Request : public Process {
public:
    void Behavior() {
        double arrival_time = Time;
        bool assigned = false;

        while (!assigned) {
            // Najdeme kontejner s nejnižší zátěží
            double min_load = std::numeric_limits<double>::max();
            Container* selected_container = nullptr;

            for (int i = 0; i < max_containers_created; i++) {
                if (containers[i]->is_active && containers[i]->is_ready && containers[i]->load < min_load) {
                    min_load = containers[i]->load;
                    selected_container = containers[i];
                }
            }

            if (selected_container != nullptr) {
                // Přijmeme požadavek do vybraného kontejneru
                selected_container->AcceptRequest();

                // Výpočet doby zpracování na základě zátěže
                double processing_time = SERVICE_TIME * (1 + ALPHA * selected_container->load);

                // Simulace zpracování požadavku
                Wait(processing_time);

                selected_container->ReleaseRequest();

                // Zaznamenáme statistiky
                double response_time = Time - arrival_time;
                response_time_stat(response_time);
                response_time_hist(response_time);
                if (response_time > SLA_RESPONSE_TIME) {
                    sla_violations++;
                }

                total_requests++;

                assigned = true;
            } else {
                // Pokud žádný kontejner není dostupný, počkáme a zkusíme znovu
                Wait(0.01);
            }
        }
    }
};

/* FUNKCE PRO GENEROVÁNÍ INTERVALU MEZI PŘÍCHODY POŽADAVKŮ */
double GetInterarrivalTime() {
    int current_interval = ((int)(Time / 60)) % 1440; // 60 sekund = 1 minuta
    int requests_in_interval = REQUESTS_MULTIPLIER * requests_per_minute[current_interval];
    double interarrival_time = 60.0 / requests_in_interval;
    return Exponential(interarrival_time);
}

void GeneratePredictedLoad() {
    // Posuneme predikci o jednu minutu dopředu
    for (int i = 0; i < 1439; i++) {
        predicted_load[i] =  REQUESTS_MULTIPLIER * requests_per_minute[i + 1];
    }
    predicted_load[1439] = REQUESTS_MULTIPLIER * requests_per_minute[0]; // Poslední interval
}


/* GENERÁTOR POŽADAVKŮ */
class RequestGenerator : public Event {
    void Behavior() {
        (new Request)->Activate();
        Activate(Time + GetInterarrivalTime());
    }
};

/* FUNKCE PRO PŘIDÁNÍ A ODEBRÁNÍ KONTEJNERŮ */
void AddContainer() {
    if (total_containers < MAX_CONTAINERS) {
        Container* container_to_activate = nullptr;
        // Vyhledáme neaktivní kontejner k reaktivaci
        for (int i = 0; i < max_containers_created; i++) {
            if (!containers[i]->is_active) {
                container_to_activate = containers[i];
                break;
            }
        }

        if (container_to_activate != nullptr) {
            // Reaktivujeme existující kontejner
            container_to_activate->Activate();
            (new ContainerStartup(container_to_activate))->Activate(Time + CONTAINER_STARTUP_TIME);
            cout << "Reaktivuji kontejner " << container_to_activate->id << ", bude připraven v čase " << Time + CONTAINER_STARTUP_TIME << endl;
        } else if (max_containers_created < MAX_CONTAINERS) {
            // Vytvoříme nový kontejner
            int id = max_containers_created;
            containers[id] = new Container(id);
            max_containers_created++;
            containers[id]->Activate();
            (new ContainerStartup(containers[id]))->Activate(Time + CONTAINER_STARTUP_TIME);
            cout << "Spouštím nový kontejner " << id << ", bude připraven v čase " << Time + CONTAINER_STARTUP_TIME << endl;
        } else {
            // Nelze přidat další kontejnery
            cout << "Nelze přidat další kontejnery, dosažen maximální počet." << endl;
            return;
        }
        total_containers++;
    }
}


void RemoveContainer() {
    if (total_containers > MIN_CONTAINERS) {
        // Najdeme aktivní kontejner s nejvyšším ID k deaktivaci
        for (int i = max_containers_created - 1; i >= 0; i--) {
            if (containers[i]->is_active) {
                containers[i]->Deactivate();
                total_containers--;
                cout << "Deaktivuji kontejner " << containers[i]->id << " v čase " << Time << endl;
                break;
            }
        }
    }
}


/* AUTOSCALERY */
class PredictiveAutoscaler : public Event {
    void Behavior() {
        int next_interval = ((int)(Time / 60) + 1) % 1440;
        int predicted_requests = predicted_load[next_interval];

        // Počet požadavků za SCALING_INTERVAL
        double requests_in_interval = predicted_requests * (SCALING_INTERVAL / 60.0);

        // Odhadnutý počet potřebných kontejnerů
        const double DESIRED_LOAD = 38.0; // Nastavte požadovanou průměrnou zátěž
        int required_containers = ceil((requests_in_interval * SERVICE_TIME) / (SCALING_INTERVAL * DESIRED_LOAD));

        // Zajištění minimálního a maximálního počtu kontejnerů
        required_containers = max(required_containers, MIN_CONTAINERS);
        required_containers = min(required_containers, MAX_CONTAINERS);

        // Implementace hystereze
        const int SCALE_UP_THRESHOLD = 1;
        const int SCALE_DOWN_THRESHOLD = 1;

        if (required_containers > total_containers + SCALE_UP_THRESHOLD) {
            int containers_to_add = min(required_containers - total_containers, MAX_CONTAINERS - total_containers);
            for (int i = 0; i < containers_to_add; ++i) {
                AddContainer();
            }
            cout << "Prediktivní škálování nahoru na " << total_containers << " kontejnerů v čase " << Time << endl;
        } else if (required_containers < total_containers - SCALE_DOWN_THRESHOLD) {
            int containers_to_remove = min(total_containers - required_containers, total_containers - MIN_CONTAINERS);
            for (int i = 0; i < containers_to_remove; ++i) {
                RemoveContainer();
            }
            cout << "Prediktivní škálování dolů na " << total_containers << " kontejnerů v čase " << Time << endl;
        }

        Activate(Time + SCALING_INTERVAL);
    }
};


class ReactiveAutoscaler : public Event {
public:
    void Behavior() {
        // Spočítáme průměrnou zátěž pouze z připravených kontejnerů
        double total_load = 0.0;
        int active_ready_containers = 0;
        for (int i = 0; i < max_containers_created; i++) {
            if (containers[i]->is_active && containers[i]->is_ready) {
                total_load += containers[i]->load;
                active_ready_containers++;
            }
        }

        double average_load = 0.0;
        if (active_ready_containers > 0) {
            average_load = total_load / active_ready_containers;
        }

        // Počet kontejnerů ve spouštění
        int starting_containers = 0;
        for (int i = 0; i < max_containers_created; i++) {
            if (containers[i]->is_active && !containers[i]->is_ready) {
                starting_containers++;
            }
        }

        // Škálování nahoru
        if (average_load > SCALE_UP_LOAD && total_containers < MAX_CONTAINERS) {
            int containers_to_add = 1; // Můžete upravit podle potřeby
            // Zohledníme počet kontejnerů ve spouštění
            int available_slots = MAX_CONTAINERS - total_containers;
            containers_to_add = min(containers_to_add, available_slots);

            if (starting_containers == 0) {
                for (int i = 0; i < containers_to_add; ++i) {
                    AddContainer();
                }
                cout << "Reaktivní škálování nahoru na " << total_containers << " kontejnerů v čase " << Time << endl;
            } else {
                cout << "Čekám na spuštění kontejnerů, již se spouští " << starting_containers << " kontejnerů." << endl;
            }
        }

        // Škálování dolů
        else if (average_load < SCALE_DOWN_LOAD && total_containers > MIN_CONTAINERS) {
            RemoveContainer();
            cout << "Reaktivní škálování dolů na " << total_containers << " kontejnerů v čase " << Time << endl;
        }

        Activate(Time + SCALING_INTERVAL);
    }
};


/* INITIALIZACE KONTEJNERŮ */
void InitContainers(int count) {
    for (int i = 0; i < count; i++) {
        containers[i] = new Container(i);
        containers[i]->is_active = true;
        containers[i]->is_ready = true;
        total_containers++;
        max_containers_created++;
    }
}

/* HLAVNÍ FUNKCE */
int main() {
    // Inicializace simulace
    Init(0, SIMULATION_TIME);
    RandomSeed(time(NULL));

    // Inicializace pole containers na nullptr
    for (int i = 0; i < MAX_CONTAINERS; i++) {
        containers[i] = nullptr;
    }

    // Generování predikované zátěže
    GeneratePredictedLoad();

    // Inicializace kontejnerů
    InitContainers(MIN_CONTAINERS);

    // Spuštění generátoru požadavků
    (new RequestGenerator)->Activate();

    // Spuštění autoscaleru
    (new ReactiveAutoscaler)->Activate();
    //(new PredictiveAutoscaler)->Activate();

    // Spuštění simulace
    Run();

    // Výstup výsledků
    response_time_stat.Output();
    response_time_hist.Output();

    double sla_percentage = 100.0 * (1 - ((double)sla_violations / total_requests));
    cout << "SLA splněno pro " << sla_percentage << "% požadavků." << endl;

    // Výstup statistik zátěže kontejnerů
    for (int i = 0; i < max_containers_created; i++) {
        cout << "Kontejner " << i << " průměrná zátěž: " << containers[i]->load_stat->MeanValue() << endl;
    }

    // Výpočet a výstup celkových nákladů
    double total_operating_cost = 0.0;
    for (int i = 0; i < max_containers_created; i++) {
        // Pokud je kontejner stále aktivní na konci simulace, přičteme zbývající čas
        if (containers[i]->is_active) {
            containers[i]->total_active_time += Time - containers[i]->activation_time;
        }
        double active_hours = containers[i]->total_active_time / 3600.0;
        double container_cost = active_hours * COST_PER_CONTAINER;
        total_operating_cost += container_cost;
    }
    cout << "Celkové náklady na provoz: " << total_operating_cost << endl;

    // Uvolnění paměti
    for (int i = 0; i < max_containers_created; i++) {
        delete containers[i]->load_stat;
        delete containers[i];
        containers[i] = nullptr;
    }

    return 0;
}
