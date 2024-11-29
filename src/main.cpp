/*
 *  Název programu: Simulace Škálování - Reaktivní vs Prediktivní přístup
 * 
 *  Autors: Jakub Fukala - xfukal01
 *           Adam Kozubek - xkozub09
 * 
 *  Datum vytvoření: 29.11.2024
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <limits>

#include "simlib.h"
#include "dataset.h"

using namespace std;


/* __PARAMETRY SIMULACE_______________________________________________________________________________________________________ */

/* --Obecné parametry-- */
const int REQUESTS_MULTIPLIER = 25;            // Násobitel počtu požadavků

const string SCALING_MODEL == "REACTIVE"       // <<< Choose the scaling model <<<
//const string SCALING_MODEL == "PREDICTIVE"

const int MAX_CONTAINERS = 40;                 // MAX a MIN počet dostupných kontejnerů
const int MIN_CONTAINERS = 1;
const int SCALING_INTERVAL = 2;                // Interval kontroly pro škálování (v minutách)
const double SLA_RESPONSE_TIME = 0.2;          // SLA: 95% požadavků musí být obslouženo do 200 ms
const double COST_PER_CONTAINER = 0.1;         // Náklady na jeden kontejner za hodinu
const double ALPHA = 0.02;                     // Koeficient ovlivňující nárůst latence

/* --Parametry pro REACTIVE-- */
const int SCALE_UP_LOAD_PERCENTAGE = 90;       // Prahová hodnota pro reaktivní škálování nahoru (průměrná zátěž v %)
const int SCALE_DOWN_LOAD_PERCENTAGE = 40;     // Prahová hodnota pro reaktivní škálování dolů (průměrná zátěž v %)

/* --Parametry pro PREDICTIVE-- */
const double DESIRED_PERCENTAGE_LOAD = 85;     // Maximální průměrná zátěž kontejneru v % pro prediktivní škálování
const int SCALE_UP_THRESHOLD = 1;              // Minimální počet nových kontejnerů pro aktivaci přeškálování
const int SCALE_DOWN_THRESHOLD = 1;            // Minimální počet odstavovaných kontejnerů pro aktivaci přeškálování
const int CONTAINER_STARTUP_TIME = 15;         // Doba spuštění kontejneru v sekundách

/* ___________________________________________________________________________________________________________________________ */


/* KONSTANTY */
const int SIMULATION_TIME = 24 * 3600;         // Simulujeme 24 hodin (v sekundách)
const int SIMULATION_INTERVAL = 60;            // Simulujeme zátěž po minutových intervalech (v sekundách)
const double SERVICE_TIME = 0.1;               // Základní čas zpracování jednoho požadavku (v sekundách)

// Predictive - Výpočet DESIRED_LOAD - maximální průměrný počet současně se vyřizujících požadavků v jednom kontejneru
const int DESIRED_LOAD = ((((DESIRED_PERCENTAGE_LOAD / 100) * SLA_RESPONSE_TIME) / SERVICE_TIME) - 1) / ALPHA;
// Reactive - Výpočet SCALE_UP_LOAD a SCALE_DOWN_LOAD - okraje intervalu, mimo nějž dochází k přeškálování
const int SCALE_UP_LOAD = ((((SCALE_UP_LOAD_PERCENTAGE / 100) * SLA_RESPONSE_TIME) / SERVICE_TIME) - 1) / ALPHA;
const int SCALE_DOWN_LOAD = ((((SCALE_DOWN_LOAD_PERCENTAGE / 100) * SLA_RESPONSE_TIME) / SERVICE_TIME) - 1) / ALPHA;


/* GLOBÁLNÍ PROMĚNNÉ */
int total_containers = 0;
int max_containers_created = 0;
int sla_violations = 0;
long total_requests = 0;
double operating_cost = 0.0;


/* PREDIKTIVNÍ ŠKÁLOVÁNÍ */
vector<int> predicted_load((SIMULATION_TIME / SIMULATION_INTERVAL), 0);

/* REÁLNÁ ZÁTĚŽ */
vector<int> real_requests_per_minute((SIMULATION_TIME / SIMULATION_INTERVAL), 0);

/* STATISTIKY */
Stat response_time_stat("Doba odezvy");
Histogram response_time_hist("Histogram doby odezvy", 0, 0.05, 20);

/* POMOCNÁ FUNKCE */
string PrintTime(int timeInSeconds);


/* Před deklarací třídy Container deklarujeme pole containers */
class Container; // Předběžná deklarace třídy
Container* containers[MAX_CONTAINERS];    // Pole kontejnerů


/* TŘÍDA KONTEJNERU */
class Container {
public:
    int id;                   // Identifikátor kontejneru
    int active_requests;      // Počet aktivních požadavků
    double load;              // Zátěž kontejneru (počet aktivních požadavků)
    Stat* load_stat;          // Statistika zátěže
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
        cout << "Kontejner " << container->id << " je připraven v čase " << PrintTime(Time) << endl;
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
    int current_interval = ((int)(Time / SIMULATION_INTERVAL)) % (SIMULATION_TIME / SIMULATION_INTERVAL);
    int requests_in_interval = real_requests_per_minute[current_interval];
    double interarrival_time = SIMULATION_INTERVAL * 1.0 / requests_in_interval;
    return Exponential(interarrival_time);
}

/* FUNKCE PRO GENEROVÁNÍ REÁLNÉ ZÁTĚŽE */
void GenerateRealRequestsPerMinute() {
    for (int i = 0; i < (SIMULATION_TIME / SIMULATION_INTERVAL); i++) {

        // Generování reálné zátěže s normálním rozdělením
        int mean = REQUESTS_MULTIPLIER * requests_per_minute[i];

        // 15% odchylka od průměrné hodnoty
        double deviation = 0.15 * mean;

        // Vygenerování náhodného počtu požadavků
        double real_requests = Normal(mean, deviation);

        // Zaokrouhlení na celé číslo a omezení na nezáporné hodnoty
        real_requests_per_minute[i] = static_cast<int>(max(0.0,real_requests));
    }
    predicted_load[((SIMULATION_TIME / SIMULATION_INTERVAL) - 1)] = REQUESTS_MULTIPLIER * requests_per_minute[0]; // Poslední interval
}

/* FUNKCE PRO GENEROVÁNÍ PREDIKOVANÉ ZÁTĚŽE */
void GeneratePredictedLoad() {
    for (int i = 0; i < (SIMULATION_TIME / SIMULATION_INTERVAL); i++) {
        predicted_load[i] =  REQUESTS_MULTIPLIER * requests_per_minute[i];
    }
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
            cout << "Reaktivuji kontejner " << container_to_activate->id << ", bude připraven v čase " << PrintTime(Time + CONTAINER_STARTUP_TIME) << endl;
        } else if (max_containers_created < MAX_CONTAINERS) {
            // Vytvoříme nový kontejner
            int id = max_containers_created;
            containers[id] = new Container(id);
            max_containers_created++;
            containers[id]->Activate();
            (new ContainerStartup(containers[id]))->Activate(Time + CONTAINER_STARTUP_TIME);
            cout << "Spouštím nový kontejner " << id << ", bude připraven v čase " << PrintTime(Time + CONTAINER_STARTUP_TIME) << endl;
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
                cout << "Deaktivuji kontejner " << containers[i]->id << " v čase " << PrintTime(Time) << endl;
                break;
            }
        }
    }
}



/* AUTOSCALERY */

/* PREDIKTIVNÍ METODA */
class PredictiveAutoscaler : public Event {
    void Behavior() {
        int next_interval = ((int)(Time / SIMULATION_INTERVAL) + 1) % (SIMULATION_TIME / SIMULATION_INTERVAL);

        /* Výpočet maximálního množství požadavků za minutu 
        v horizontu SCALING_INTERVAL (době zpřístupnění nového kontejneru) + CONTAINER_STARTUP_TIME (doba vytvoření nového kontejneru)*/
        int max_predicted_requests = 0;
        for (int i = floor(CONTAINER_STARTUP_TIME / SIMULATION_INTERVAL); i < (SCALING_INTERVAL + ceil(CONTAINER_STARTUP_TIME / SIMULATION_INTERVAL)); ++i) {
            int idx = min(((next_interval + i) % (SIMULATION_TIME / SIMULATION_INTERVAL)), ((SIMULATION_TIME / SIMULATION_INTERVAL) - 1)) ;
            if (max_predicted_requests < predicted_load[idx]){
                max_predicted_requests = predicted_load[idx];
            }
        }
        max_predicted_requests = max_predicted_requests / SIMULATION_INTERVAL;

        double average_procesing_time = SERVICE_TIME * (1 + (ALPHA * DESIRED_LOAD));
        // Odhadnutý počet potřebných kontejnerů
        int required_containers = ((max_predicted_requests * average_procesing_time) / (DESIRED_LOAD)) + 1;

        // Zajištění minimálního a maximálního počtu kontejnerů
        required_containers = max(required_containers, MIN_CONTAINERS);
        required_containers = min(required_containers, MAX_CONTAINERS);

        // Škálování o požadovaný počet kontejnerů
        if (required_containers >= total_containers + SCALE_UP_THRESHOLD) {
            int containers_to_add = min(required_containers - total_containers, MAX_CONTAINERS - total_containers);
            for (int i = 0; i < containers_to_add; ++i) {
                AddContainer();
            }
            cout << "Prediktivní škálování nahoru na " << total_containers << " kontejnerů v čase " << PrintTime(Time) << endl;
        } else if (required_containers <= total_containers - SCALE_DOWN_THRESHOLD) {
            int containers_to_remove = min(total_containers - required_containers, total_containers - MIN_CONTAINERS);
            for (int i = 0; i < containers_to_remove; ++i) {
                RemoveContainer();
            }
            cout << "Prediktivní škálování dolů na " << total_containers << " kontejnerů v čase " << PrintTime(Time) << endl;
        }
        
        // Uspání po čas další kontroly
        Activate(Time + SCALING_INTERVAL*60);
    }
};


/* REAKTIVNÍ METODA */
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
                cout << "Reaktivní škálování nahoru na " << total_containers << " kontejnerů v čase " << PrintTime(Time) << endl;
            } else {
                cout << "Čekám na spuštění kontejnerů, již se spouští " << starting_containers << " kontejnerů." << endl;
            }
        }

        // Škálování dolů
        else if (average_load < SCALE_DOWN_LOAD && total_containers > MIN_CONTAINERS) {
            RemoveContainer();
            cout << "Reaktivní škálování dolů na " << total_containers << " kontejnerů v čase " << PrintTime(Time) << endl;
        }
        
        // Uspání po čas další kontroly
        Activate(Time + SCALING_INTERVAL*60);
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
    
    // Generování reálné zátěže
    GenerateRealRequestsPerMinute();

    // Inicializace kontejnerů
    InitContainers(MIN_CONTAINERS);

    // Spuštění generátoru požadavků
    (new RequestGenerator)->Activate();

    // Spuštění autoscaleru
    if (SCALING_MODEL == "REACTIVE")
        (new ReactiveAutoscaler)->Activate();
    else if (SCALING_MODEL == "PREDICTIVE")
        (new PredictiveAutoscaler)->Activate();
    else{
        cerr << "Please choose the scaling model (\"REACTIVE\" or \"PREDICTIVE\")";
        return 1;
    }

    // Spuštění simulace
    Run();

    // Výstup výsledků
    //response_time_stat.Output();
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
/* konec MAIN */


// Pomocná funkce pro převod času (sekundy) na formátovaný řetězec hh:mm:ss
string PrintTime(int timeInSeconds) {
    int hours = static_cast<int>(timeInSeconds) / 3600;
    int minutes = (static_cast<int>(timeInSeconds) % 3600) / 60;
    int seconds = static_cast<int>(timeInSeconds) % 60;

    ostringstream formattedTime;
    formattedTime << setw(2) << setfill('0') << hours << ":"
                  << setw(2) << setfill('0') << minutes << ":"
                  << setw(2) << setfill('0') << seconds;

    return formattedTime.str();
}
