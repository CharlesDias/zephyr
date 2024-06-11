# Executar o MQTT no Simulador nativo

1. Acessar a pasta `~/VIRTUS-CC/net-tools`. Caso não tenha o projeto `net-tools`, clone o repositório https://github.com/zephyrproject-rtos/net-tools/tree/master

2. Abrir o terminal e executar o script `net-setup.sh` que se encontra na pasta `net-tools`.

3. Executar o mosquitto

    ```bash
    sudo systemctl enable mosquitto
    sudo systemctl start mosquitto
    ```

4. Compilar o projeto para o simulador nativo e executar

    ```bash
    west build -p -b native_sim/native/64 samples/net/mqtt_ethernet/
    west build -t run
    ```
