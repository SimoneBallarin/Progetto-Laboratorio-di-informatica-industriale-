#include <stdio.h>
#include "parser.h"

/*
 * Programma dimostrativo: NON e' il simulatore finale, serve solo a far
 * vedere che il modulo di parsing funziona su tutti i file di esempio,
 * sia validi che con errori intenzionali.
 */
int main(void) {

    printf("=== 1. Configurazione impianto VALIDA ===\n");
    {
        PlantConfig cfg;
        ParseSummary s = parse_plant_config("examples/plant_config_valid.txt", &cfg);
        print_parse_summary(&s);
        printf("  buffer=%d conveyor=%d station=%d sensor=%d dispatcher=%d\n",
               cfg.buffer_count, cfg.conveyor_count, cfg.station_count,
               cfg.sensor_count, cfg.dispatcher_count);
    }

    printf("\n=== 2. Configurazione impianto CON ERRORI ===\n");
    {
        PlantConfig cfg;
        ParseSummary s = parse_plant_config("examples/plant_config_with_errors.txt", &cfg);
        print_parse_summary(&s);
    }

    printf("\n=== 3. File oggetti NOMINALE ===\n");
    {
        ObjectRecord objs[64];
        int count = 0;
        ParseSummary s = parse_objects_file("examples/objects_nominal.txt", objs, 64, &count);
        print_parse_summary(&s);
        for (int i = 0; i < count; i++) {
            printf("    oggetto %s: arrivo=%.1f tipo=%s priorita=%d\n",
                   objs[i].id, objs[i].arrival, objs[i].type, objs[i].priority);
        }
    }

    printf("\n=== 4. File oggetti CON ERRORI ===\n");
    {
        ObjectRecord objs[64];
        int count = 0;
        ParseSummary s = parse_objects_file("examples/objects_with_errors.txt", objs, 64, &count);
        print_parse_summary(&s);
    }

    printf("\n=== 5. Scenario NOMINALE ===\n");
    {
        ScenarioConfig sc;
        ParseSummary s = parse_scenario_file("examples/scenario_nominal.txt", &sc);
        print_parse_summary(&s);
        printf("  nome=%s load_multiplier=%.2f fault_enabled=%d\n",
               sc.name, sc.load_multiplier, sc.fault_enabled);
    }

    printf("\n=== 6. Scenario DIFFICILE ===\n");
    {
        ScenarioConfig sc;
        ParseSummary s = parse_scenario_file("examples/scenario_difficult.txt", &sc);
        print_parse_summary(&s);
        printf("  nome=%s load_multiplier=%.2f fault_enabled=%d fault_sensor=%s [%d-%d]\n",
               sc.name, sc.load_multiplier, sc.fault_enabled,
               sc.fault_sensor_id, sc.fault_start_step, sc.fault_end_step);
    }

    return 0;
}