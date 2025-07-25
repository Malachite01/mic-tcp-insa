#include <mictcp.h>
#include <api/mictcp_core.h>

//! Parametres globaux définis dans mictcp.h
sliding_window_t loss_window[MAX_SOCKETS]; // Fenêtre glissante pour chaque socket
int real_loss_rate = REAL_LOSS; // Taux de perte réel utilisé pour simuler les pertes
// Taux de perte acceptables. Par défaut (20%)
int acceptable_loss_rate =  DEFAULT_ACCEPTABLE_LOSS; // Utilisée pour négocier le taux de perte acceptable dans le SYN de connexion 

mic_tcp_sock socket_list[MAX_SOCKETS]; //Liste des sockets MIC-TCP 
int last_used_socket = 0; // Dernier socket utilisé
int next_sequence[MAX_SOCKETS] = {0}; // Numéro de séquence du prochain PDU à émettre

/*
 * Fonction pour afficher le nom de la fonction passée en paramètre
 */
void print_func_name(const char* func_name) {
   printf("[MIC-TCP] Appel de la fonction: %s\n", func_name);
}


//!     ___________________________
//!    |_PARTIE_FENETRE_GLISSANTE_| (structure définie dans mictcp.h)

/*
 * Initialise la fenêtre glissante pour un socket
 */
void init_a_sliding_window(int socket) {
   print_func_name(__FUNCTION__);
   // A l'adresse du descripteur socket, on initialise la fenêtre glissante
   sliding_window_t *window = &loss_window[socket];
   
   // Initialiser tous les éléments à 0
   for (int i = 0; i < WINDOW_SIZE; i++) {
      window->sent_packets[i] = 0;
      window->ack_received[i] = 0;
   }
   
   window->window_index = 0;
   window->packets_in_window = 0;
   
   printf("[MIC-TCP] Fenêtre glissante initialisée pour socket %d\n", socket);
}

/*
 * Ajoute un paquet envoyé dans la fenêtre glissante
 */
void add_sent_packet(int socket) {
   // A l'adresse de socket, on ajoute un paquet envoyé dans la fenêtre glissante
   sliding_window_t *window = &loss_window[socket];
   
   // Ajouter le paquet à la position courante
   window->sent_packets[window->window_index] = 1;
   window->ack_received[window->window_index] = 0; // Pas encore d'ACK
   
   // Avancer l'index (circulaire car modulo WINDOW_SIZE)
   window->window_index = (window->window_index + 1) % WINDOW_SIZE;
   
   // Incrémenter le nombre de paquets si la fenêtre n'est pas pleine
   if (window->packets_in_window < WINDOW_SIZE) window->packets_in_window++;
   
   printf("[MIC-TCP] Socket %d: Paquet ajouté à la fenêtre (total: %d)\n", socket, window->packets_in_window);
}

/*
 * Marque un ACK comme reçu dans la fenêtre glissante
 */
void mark_ack_received(int socket) {
   // A l'adresse de socket, on marque un ACK comme reçu dans la fenêtre glissante
   sliding_window_t *window = &loss_window[socket];
   
   // Marquer l'ACK pour le dernier paquet envoyé
   int last_sent_index = (window->window_index - 1 + WINDOW_SIZE) % WINDOW_SIZE; // Modulo pour gérer l'index circulaire
   window->ack_received[last_sent_index] = 1;
   
   printf("[MIC-TCP] Socket %d: ACK marqué comme reçu\n", socket);
}

/*
 * Calcule le taux de perte dans la fenêtre glissante actuelle
 * Retourne le pourcentage de perte (0-100)
 */
int calculate_current_loss_rate(int socket) {
   sliding_window_t *window = &loss_window[socket];
   
   if (window->packets_in_window == 0) return 0; // Pas de paquets envoyés
   
   int sent_count = 0, ack_count = 0;
   
   // Compter les paquets envoyés et les ACK reçus
   for (int i = 0; i < window->packets_in_window; i++) {
      if (window->sent_packets[i] == 1) {
         sent_count++;
         if (window->ack_received[i] == 1) {
            ack_count++;
         }
      }
   }
   
   if (sent_count == 0) return 0; // Pas de paquets envoyés, pas de perte
   // Calculer le taux de perte
   int loss_rate_percent = ((sent_count - ack_count) * 100) / sent_count;
   
   printf("[MIC-TCP] Socket %d: Taux de perte calculé: %d%% (%d perdus sur %d)\n", socket, loss_rate_percent, sent_count - ack_count, sent_count);
   return loss_rate_percent;
}

/*
 * Évalue si on peut accepter les pertes actuelles
 * Retourne 0 si on peut "mentir" sur le numéro de séquence, -1 sinon
 */
int can_accept_loss(int socket) { 
   int current_loss_rate = calculate_current_loss_rate(socket);
   
   if (current_loss_rate <= acceptable_loss_rate) return 0; // On peut accepter la perte
   return -1; // Doit continuer à attendre l'ACK
}

/*
 * Affiche le contenu de la fenêtre glissante pour le socket donné
 */
void debug_window(int socket) {
   sliding_window_t *window = &loss_window[socket];
   printf("[MIC-TCP] Fenêtre glissante pour le socket %d:\n", socket);
   printf("  Index courant: %d\n", window->window_index);
   printf("  Paquets dans la fenêtre: %d\n", window->packets_in_window);
   printf("  Paquets envoyés: ");
   for (int i = 0; i < WINDOW_SIZE; i++) {
      printf("%d ", window->sent_packets[i]);
   }
   printf("\n  ACK reçus: ");
   for (int i = 0; i < WINDOW_SIZE; i++) {
      printf("%d ", window->ack_received[i]);
   }
   printf("\n");
}

//!     _______________________
//!    |_PARTIE_VERIFICATIONS_|

/*
 * Fonction de vérification de la validité d'un socket
 * Retourne 0 si le socket est valide, -1 sinon
 */
int verif_socket(int socket) {
   if (socket < 0 || socket > last_used_socket) {
      printf("[MIC-TCP] Erreur: Socket invalide\n");
      return -1;
   }
   return 0;
}

/*
 * Fonction de vérification de la validité d'une adresse
 * Retourne 0 si l'adresse est valide, -1 sinon
 */
int verif_address(mic_tcp_sock_addr addr) {
   // Vérifie que le port est valide (> 1024)
   if (addr.port <= 1024) return -1;

   if(strstr(addr.ip_addr.addr, "localhost")) return 0; // Si l'adresse est "localhost", on considère que c'est valide

   // Vérifie que la taille de l'adresse IP est non nulle
   if (addr.ip_addr.addr_size == 0 || addr.ip_addr.addr == NULL) return -1;

   // On copie l'adresse IP
   char ip_copy[addr.ip_addr.addr_size + 1];
   strncpy(ip_copy, addr.ip_addr.addr, addr.ip_addr.addr_size);
   ip_copy[addr.ip_addr.addr_size] = '\0'; // Ajoute le caractère de fin de chaîne

   // Compte le nombre de segments dans l'adresse IP
   int segments = 0;
   char* token = strtok(ip_copy, "."); // On découpe l'adresse IP en segments

   while (token != NULL) {
      // Convertit le segment en entier
      char* endptr;
      long value = strtol(token, &endptr, 10);

      // Vérifie que la conversion est valide et dans l'intervalle 0-255
      if (*endptr != '\0' || value < 0 || value > 255) return -1;

      segments++;
      token = strtok(NULL, ".");
   }

   // Vérifie qu'on a exactement 4 segments (X.X.X.X) pour IPv4
   if (segments != 4) return -1;

   return 0;
}

//!     _______________________________
//!    |_PARTIE_FONCTIONS_PRINCIPALES_|

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm){
   int result = -1;
   print_func_name(__FUNCTION__);
   result = initialize_components(sm); /* Appel obligatoire */
   set_loss_rate(real_loss_rate); /* On initialise le taux de perte, c'est la fonction IP_send() qui gère */
   
   // Verifie si la liste de sockets est pleine
   if (last_used_socket > MAX_SOCKETS) return -1;
   
   socket_list[last_used_socket].fd = last_used_socket;
   socket_list[last_used_socket].state = CLOSED;
   
   // Initialiser la fenêtre glissante pour ce socket
   init_a_sliding_window(last_used_socket);
   
   int socket = last_used_socket; // On récupère le descripteur du socket
   last_used_socket++;

   // Retourne le descripteur du socket ou -1 en cas d'erreur
   return result == -1 ? result : socket;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr) {
   print_func_name(__FUNCTION__);
   
   //Vérifie si le socket est valide et si l'adresse est valide
   if (verif_socket(socket) == 0) {
      // Attribue addr au socket
      socket_list[socket].local_addr = addr; /* On attribue l'adresse au socket */
      printf("[MIC-TCP] Socket %d lié à l'adresse %s:%d\n", socket, addr.ip_addr.addr, addr.port);
      return 0;
   }
   return -1;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr) {
   print_func_name(__FUNCTION__);
   
   // Vérifie si le socket est valide
   if (verif_socket(socket) == -1) return -1;
   
   //? Met le socket en état d'acceptation de connexions
   socket_list[socket].state = IDLE; // On change l'état du socket
   printf("[MIC-TCP] Socket %d en attente de connexion...\n", socket);

   // On utilise un mutex et une condition pour attendre que la connexion soit établie
   pthread_mutex_lock(&socket_list[socket].mutex);
   
   //? Attente passive jusqu'à ce qu'une connexion soit établie
   while(socket_list[socket].state != ESTABLISHED) {
      // Le thread se bloque jusqu'à ce qu'il soit réveillé
      pthread_cond_wait(&socket_list[socket].cond, &socket_list[socket].mutex);
   }
   
   pthread_mutex_unlock(&socket_list[socket].mutex);
   
   return 0; // Retourne 0 si la connexion est établie
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr) {
   print_func_name(__FUNCTION__);
   // Vérifie si le socket est valide et si l'adresse est valide
   if (verif_socket(socket) == -1 || verif_address(addr) == -1) return -1;

   socket_list[socket].state = IDLE;
   // Assigner l'adresse distante au socket
   socket_list[socket].remote_addr = addr;

   //? Tant que la connexion n'est pas établie (pas de ACK), on envoie un SYN
   while (socket_list[socket].state != ESTABLISHED) {
      //? Envoi d'un SYN pour établir la connexion
      mic_tcp_pdu pdu_syn;
      pdu_syn.header.source_port = socket_list[socket].local_addr.port; 
      pdu_syn.header.dest_port = addr.port;
      pdu_syn.header.syn = 1; 
      pdu_syn.header.ack = 0; 
      pdu_syn.header.fin = 0;
      pdu_syn.payload.size = 0;
      //?  Le client transmet le taux acceptable de perte dans un champ innexistant du PDU
      pdu_syn.header.ack_num = acceptable_loss_rate;

      printf("[MIC-TCP] Envoi du SYN pour établir la connexion sur le socket %d\n", socket);
      
      if (IP_send(pdu_syn, addr.ip_addr) == -1) return -1; 

      mic_tcp_pdu pdu_syn_ack;
      mic_tcp_ip_addr local_addr_ack, remote_addr_ack;
      local_addr_ack.addr = malloc(sizeof(char) * 16); 
      local_addr_ack.addr_size = 16; 
      remote_addr_ack.addr = malloc(sizeof(char) * 16); 
      remote_addr_ack.addr_size = 16;
      pdu_syn_ack.payload.size = 0; 

      //? On attend un SYN-ACK en réponse, 3 fois MAX_TIMEOUT car SYN, SYN_ACK, ACK
      int recv_status = IP_recv(&pdu_syn_ack, &local_addr_ack, &remote_addr_ack, 3*MAX_TIMEOUT); // Attente du SYN-ACK

      //? Si on reçoit un SYN-ACK, on envoie un ACK pour finaliser la connexion
      if (recv_status != -1 && pdu_syn_ack.header.syn == 1 && pdu_syn_ack.header.ack == 1) {
         printf("SYN-ACK reçu pour le socket %d\n", socket);
         // On a reçu un SYN-ACK, on envoie un ACK pour finaliser la connexion
         // On réutilise le PDU pdu_syn pour envoyer l'ACK (pour éviter de créer un nouveau PDU)
         pdu_syn.header.ack = 1; 
         pdu_syn.header.syn = 0; 
         if (IP_send(pdu_syn, addr.ip_addr) == -1) return -1; // Envoi de l'ACK
         pthread_mutex_lock(&socket_list[socket].mutex);
         socket_list[socket].state = ESTABLISHED; // On change l'état du socket
         pthread_cond_signal(&socket_list[socket].cond);  // Réveille le thread en attente
         pthread_mutex_unlock(&socket_list[socket].mutex);
      }
   }
   printf("[MIC-TCP] Connexion établie avec succès sur le socket %d\n", socket);     
   return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size) {
   print_func_name(__FUNCTION__);

   // Vérifie si le socket est valide
   if (verif_socket(mic_sock) == -1) return -1;
   
   // Création du mic_tcp_pdu qui crée automatiquement le mic_tcp_header et le mic_tcp_payload
   // Création du PDU Ack pour la réponse
   mic_tcp_pdu pdu, pdu_ack;
   int effective_ip_send = -1; // Variable pour stocker le résultat de l'envoi sur la couche IP
   //! Adresses IP locales et distantes du PDU
   mic_tcp_ip_addr local_addr_ack, remote_addr_ack; // Variables pour stocker les adresses IP locales et distantes du PDU ACK
   local_addr_ack.addr = malloc(sizeof(char) * 16); // Allocation de mémoire pour l'adresse IP locale
   local_addr_ack.addr_size = 16; // Taille de l'adresse IP locale
   remote_addr_ack.addr = malloc(sizeof(char) * 16); // Allocation de mémoire pour l'adresse IP distante
   remote_addr_ack.addr_size = 16; // Taille de l'adresse IP distante
   
   //! Remplissage du PDU HEADER
   //mettre le numero de port local source associé a mon_socket
   pdu.header.source_port = socket_list[mic_sock].local_addr.port;
   //mettre le numero de port distant destination associé a mon_socket
   pdu.header.dest_port = socket_list[mic_sock].remote_addr.port;
   // Pour le moment on ne gère pas les numéros de séquence et d'acquittement
   pdu.header.syn = 0;
   pdu.header.ack = 0;
   pdu.header.fin = 0;
   //? Remplissage du numéro de séquence
   pdu.header.seq_num = next_sequence[mic_sock]; // Numéro de séquence du PDU

   //! Remplissage du PDU PAYLOAD
   pdu.payload.data = mesg; // On met le message dans le payload
   pdu.payload.size = mesg_size; // On met la taille du message dans le payload
   pdu_ack.payload.size = 0; // Pas de données dans le PDU ACK

   int ack_received = 0; // Variable pour savoir si on a reçu un ACK valide
   int premier_envoi = 1; // Variable pour savoir si c'est le premier envoi du PDU

   //! On boucle jusqu'à ce qu'on reçoive un ACK valide
   while (ack_received == 0) { 
      //? Envoi du PDU sur la couche IP
      printf("[MIC-TCP] Envoi du PDU avec numéro de séquence : %d\n", pdu.header.seq_num);
      effective_ip_send = IP_send(pdu, socket_list[mic_sock].remote_addr.ip_addr); // On envoie le PDU sur la couche IP
      // Erreur lors de l'envoi du PDU
      if (effective_ip_send == -1) return -1;
      //? Ajouter le paquet à la fenêtre glissante
      if (premier_envoi) { // on l'ajoute qu'une seule fois
         add_sent_packet(mic_sock); 
         premier_envoi = 0; 
      }
      

      //? Attente d'un ACK avec timeout dans le cas où le PDU est perdu
      int recv_status = IP_recv(&pdu_ack, &local_addr_ack, &remote_addr_ack, MAX_TIMEOUT); // On attend le PDU ACK sur la couche IP

      //? On vérifie si le PDU_ACK reçu à un numéro de séquence valide
      // num séquence PDU_ACK = num séquence du prochain PDU à émettre + 1 (car on attend un ACK pour le PDU envoyé)
      if (recv_status != -1 
         && pdu_ack.header.ack == 1
         && pdu_ack.header.seq_num == next_sequence[mic_sock]+1)
      {
         ack_received = 1; // On a reçu un ACK valide donc on sort de la boucle
         mark_ack_received(mic_sock); // On marque l'ACK comme reçu dans la fenêtre glissante
         printf("[MIC-TCP] ACK reçu pour le PDU avec numéro de séquence : %d\n", pdu_ack.header.seq_num-1);
         next_sequence[mic_sock]++; // On incrémente le numéro de séquence du prochain PDU à émettre
      };

      //? Pas de ACK reçu ou ACK invalide
      if (recv_status == -1 || !ack_received) {
         //? Vérifier le taux de perte
         if (can_accept_loss(mic_sock) == -1) { 
            // Si le taux de perte est trop élevé on continue à attendre un ACK valide
            printf("[MIC-TCP] Taux de perte inacceptable, attente d'un ACK valide\n");
            continue; // On continue à attendre un ACK valide
         } else {
            // Taux de perte acceptable, on "ment" sur le numéro de séquence
            printf("[MIC-TCP] Perte PDU acceptable\n"); 
            ack_received = 1; // On considère qu'on a reçu un ACK pour le PDU suivant
            effective_ip_send = mesg_size; // Simuler un envoi réussi
            // On n'appelle pas mark_ack_received ici car on n'a pas reçu d'ACK valide
            // Donc pour les stats de la fenêtre glissante, on ne marque pas l'ACK comme reçu
         }
      }

      debug_window(mic_sock);
      printf("[MIC-TCP] Numéro de séquence actuel pour le socket %d\n", next_sequence[mic_sock]);
   }
   return effective_ip_send; // Retourne la taille des données envoyées (return -1 en cas d'erreur)    
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size) {
   print_func_name(__FUNCTION__);
   //verifie si le socket est valide
   if (verif_socket(socket) == -1) return -1;

   mic_tcp_payload payload;
   payload.data = mesg; // On met le message dans le payload
   payload.size = max_mesg_size; // On met la taille du message dans le payload
   // On lit le message dans le buffer de réception
   return app_buffer_get(payload); //retourne le nombre d'octets -1 si erreur
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr) {
   print_func_name(__FUNCTION__);

   //? Vérifie si le PDU était destiné à un de nos sockets
   int fd = -1;
   for (int i = 0; i < MAX_SOCKETS && i < last_used_socket; i++) {
      // Si le port local du socket correspond au port de destination du PDU
      if (socket_list[i].local_addr.port == pdu.header.dest_port) {
         fd = i; // On a trouvé le socket correspondant
         break;
      }
   }
   if (fd == -1) {
      printf("[MIC-TCP] PDU non destiné à un de nos sockets\n");
      return; //on ne fait rien si le PDU n'est pas pour nous
   }

   //! CREATION DU PDU à renvoyer, ACK, FIN ou autre (ACK par défault)
   mic_tcp_pdu pdu_ack;
   // On inverse les ports source et destination pour répondre
   pdu_ack.header.source_port = pdu.header.dest_port;
   pdu_ack.header.dest_port = pdu.header.source_port;
   pdu_ack.header.seq_num = next_sequence[fd]; // Numéro de séquence du PDU
   pdu_ack.header.ack = 1; 
   pdu_ack.header.syn = 0; 
   pdu_ack.payload.size = 0; // Pas de données dans le PDU ACK

   //! Phase d'établissement de connexion
   //? Si on recoit un SYN
   if (socket_list[fd].state != ESTABLISHED && pdu.header.syn == 1 && pdu.header.ack == 0) {
      printf("[MIC-TCP] SYN reçu, envoi du SYN-ACK\n");
      //? Mise à jour du taux de perte acceptable depuis le client
      acceptable_loss_rate = pdu.header.ack_num;
      printf("[MIC-TCP] Taux de perte accepté par le client : %d%%\n", acceptable_loss_rate);
      
      mic_tcp_pdu pdu_recv;
      pdu_recv.payload.data = 0; // On met le message dans le payload
      pdu_recv.payload.size = 0; // On met la taille du message dans le payload
      mic_tcp_ip_addr local_addr_recv, remote_addr_recv;
      local_addr_recv.addr = malloc(sizeof(char) * 16);
      local_addr_recv.addr_size = 16; 
      remote_addr_recv.addr = malloc(sizeof(char) * 16); 
      remote_addr_recv.addr_size = 16;
     
      pdu_ack.header.syn = 1; // pour le SYN-ACK

      int received = 0; 
      while (!received) {
         IP_send(pdu_ack, remote_addr); // Envoi du SYN-ACK
         printf("[MIC-TCP] Envoi du SYN-ACK pour le socket %d\n", fd);
         int result = IP_recv(&pdu_recv, &local_addr_recv, &remote_addr_recv, 3*MAX_TIMEOUT);
         printf("debug result : %d\n", result);
         // Si on recoit un ACK pour le SYN-ACK
         if (result != -1 && pdu_recv.header.ack == 1 && pdu_recv.header.syn == 0) { 
            printf("[MIC-TCP] ACK reçu pour le SYN-ACK\n");

            pthread_mutex_lock(&socket_list[fd].mutex);
            //Assigner l'adresse distante au socket
            socket_list[fd].remote_addr.port = pdu.header.source_port;
            socket_list[fd].remote_addr.ip_addr = remote_addr;
            socket_list[fd].state = ESTABLISHED; // On change l'état du socket
            pthread_cond_signal(&socket_list[fd].cond);  // Réveille le thread en attente
            pthread_mutex_unlock(&socket_list[fd].mutex);
            received = 1; // On a reçu l'ACK
            printf("[MIC-TCP] Connexion établie pour le socket %d\n", fd);
         } 
      }
   }

   //! Phase de transfert des données
   if (socket_list[fd].state == ESTABLISHED) {
      //? Verifier le num de sequence du PDU
      if (pdu.header.seq_num == next_sequence[fd] && pdu.header.ack == 0 && pdu.header.syn == 0 && pdu.header.fin == 0) {
         // On met le PDU dans le buffer de réception du socket
         app_buffer_put(pdu.payload);
         next_sequence[fd]++; // On incrémente le numéro de séquence du prochain PDU à émettre
      }

      //? Envoi de l'ACK pour le PDU reçu
      IP_send(pdu_ack, remote_addr); // Envoi de l'ACK
   }
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket) {
   print_func_name(__FUNCTION__);
   // Vérifie si le socket est valide
   if (verif_socket(socket) == -1) return -1;
   socket_list[socket].state = CLOSED; // On change l'état du socket

   // Réorganisation de la liste de sockets pour éviter les trous
   for (int i = socket; i < last_used_socket-1; i++) {
      socket_list[i] = socket_list[i + 1];
      socket_list[i].fd = i; // Met à jour le descripteur de fichier
      next_sequence[i] = next_sequence[i + 1]; // Met à jour le numéro de séquence
      loss_window[i] = loss_window[i + 1];
   }
   last_used_socket--;
   return 0;
}