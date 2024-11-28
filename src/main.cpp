#include <iostream>
#include <cmath>
#include <vector>
#include <limits>
#include "simlib.h"

using namespace std;

/* PARAMETRY SIMULACE */
const double SIMULATION_TIME = 24 * 3600; // Simulujeme 24 hodin (v sekundách)
const double SERVICE_TIME = 0.1;          // Základní čas zpracování jednoho požadavku (v sekundách)
const int MAX_CONTAINERS = 50;
const int MIN_CONTAINERS = 1;
const double SCALING_INTERVAL = 300;      // Interval kontroly pro škálování (v sekundách)
const double SLA_RESPONSE_TIME = 0.2;     // SLA: 95% požadavků musí být obslouženo do 200 ms
const double COST_PER_CONTAINER = 0.1;    // Náklady na jeden kontejner za hodinu
const double ALPHA = 0.02;                // Koeficient ovlivňující nárůst latence
const double SCALE_UP_LOAD = 8;          // Prahová hodnota pro škálování nahoru (průměrná zátěž)
const double SCALE_DOWN_LOAD = 4;         // Prahová hodnota pro škálování dolů (průměrná zátěž)

/* GLOBÁLNÍ PROMĚNNÉ */
int total_containers = 0;
int max_containers_created = 0;
int sla_violations = 0;
long total_requests = 0;
double operating_cost = 0.0;

/* DATASET ZÁTĚŽE */
int requests_per_interval[48] = {
    // Hodnoty intenzity požadavků pro každou půlhodinu
    // Například:
    2 * 25000, 2 * 22500, 2 * 20000, 2 * 17500, 2 * 15000, 2 * 15000, 2 * 17500, 2 * 20000,
    2 * 25000, 2 * 30000, 2 * 40000, 2 * 50000, 2 * 75000, 2 * 100000, 2 * 125000, 2 * 150000,
    2 * 175000, 2 * 200000, 2 * 225000, 2 * 235000, 2 * 225000, 2 * 200000, 2 * 175000, 2 * 150000,
    2 * 125000, 2 * 100000, 2 * 75000, 2 * 60000, 2 * 50000, 2 * 40000, 2 * 30000, 2 * 25000,
    2 * 22500, 2 * 20000, 2 * 17500, 2 * 15000, 2 * 15000, 2 * 17500, 2 * 20000, 2 * 25000,
    2 * 30000, 2 * 40000, 2 * 50000, 2 * 60000, 2 * 75000, 2 * 90000, 2 * 100000, 2 * 110000
};

/* PŘEDIKTIVNÍ ŠKÁLOVÁNÍ */
vector<int> predicted_load(48, 0);

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

    Container(int id) : id(id), active_requests(0), load(0.0), total_active_time(0.0), is_active(true) {
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
            is_active = true;
            activation_time = Time;
        }
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
                if (containers[i]->is_active && containers[i]->load < min_load) {
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
                // Pokud žádný kontejner není dostupný (nemělo by nastat), počkáme a zkusíme znovu
                Wait(0.01);
            }
        }
    }
};

/* FUNKCE PRO GENEROVÁNÍ INTERVALU MEZI PŘÍCHODY POŽADAVKŮ */
double GetInterarrivalTime() {
    int current_interval = ((int)(Time / 1800)) % 48; // 1800 sekund = 30 minut
    int requests_in_interval = requests_per_interval[current_interval];
    double interarrival_time = 1800.0 / requests_in_interval;
    return Exponential(interarrival_time);
}


void GeneratePredictedLoad() {
    // Posuneme predikci o jeden půlhodinový interval dopředu
    for (int i = 0; i < 47; i++) {
        predicted_load[i] = requests_per_interval[i + 1];
    }
    predicted_load[47] = requests_per_interval[0]; // Poslední interval
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
        int id = total_containers;
        if (containers[id] == nullptr) {
            containers[id] = new Container(id);
            max_containers_created++;
        } else {
            containers[id]->Activate();
        }
        total_containers++;
    }
}

void RemoveContainer() {
    if (total_containers > MIN_CONTAINERS) {
        total_containers--;
        containers[total_containers]->Deactivate();
    }
}

/* AUTOSCALERY */
class ReactiveAutoscaler : public Event {
    void Behavior() {
        // Spočítáme průměrnou zátěž
        double total_load = 0.0;
        int active_containers = 0;
        for (int i = 0; i < max_containers_created; i++) {
            if (containers[i]->is_active) {
                total_load += containers[i]->load;
                active_containers++;
            }
        }
        double average_load = total_load / active_containers;

        // Škálování nahoru
        if (average_load > SCALE_UP_LOAD && total_containers < MAX_CONTAINERS) {
            AddContainer();
            cout << "Reaktivní škálování nahoru na " << total_containers << " kontejnerů v čase " << Time << endl;
        }

        // Škálování dolů
        if (average_load < SCALE_DOWN_LOAD && total_containers > MIN_CONTAINERS) {
            RemoveContainer();
            cout << "Reaktivní škálování dolů na " << total_containers << " kontejnerů v čase " << Time << endl;
        }

        Activate(Time + SCALING_INTERVAL);
    }
};

class PredictiveAutoscaler : public Event {
    void Behavior() {
        int next_interval = ((int)(Time / 1800) + 1) % 48;
        int predicted_requests = predicted_load[next_interval];

        // Počet požadavků za SCALING_INTERVAL
        int requests_in_interval = predicted_requests * (SCALING_INTERVAL / 1800.0);

        // Odhadnutá zátěž na kontejner
        double safety_factor = 0.2; // 20% rezervy
        int required_containers = ceil((requests_in_interval * SERVICE_TIME * safety_factor) / SCALING_INTERVAL);


        if (required_containers > total_containers && total_containers < MAX_CONTAINERS) {
            while (total_containers < required_containers && total_containers < MAX_CONTAINERS) {
                AddContainer();
            }
            cout << "Prediktivní škálování nahoru na " << total_containers << " kontejnerů v čase " << Time << endl;
        } else if (required_containers < total_containers && total_containers > MIN_CONTAINERS) {
            while (total_containers > required_containers && total_containers > MIN_CONTAINERS) {
                RemoveContainer();
            }
            cout << "Prediktivní škálování dolů na " << total_containers << " kontejnerů v čase " << Time << endl;
        }

        Activate(Time + SCALING_INTERVAL);
    }
};


/* INITIALIZACE KONTEJNERŮ */
void InitContainers(int count) {
    for (int i = 0; i < count; i++) {
        containers[i] = new Container(i);
        total_containers++;
        max_containers_created++;
    }
}

/* HLAVNÍ FUNKCE */
int main() {
    // Inicializace simulace
    Init(0, SIMULATION_TIME);
    RandomSeed(time(NULL));

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
