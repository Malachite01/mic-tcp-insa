#include <mictcp.h>
#include <api/mictcp_core.h>

#define MAX_SOCKETS 1024
#define MAX_TIMEOUT 100 // Timeout pour la réception d'un ACK en µs

mic_tcp_sock socket_list[MAX_SOCKETS]; //Liste des sockets MIC-TCP 
int last_used_socket = -1; // Dernier socket utilisé
int next_sequence[MAX_SOCKETS] = {0}; // Numéro de séquence du prochain PDU à émettre


/*
 * Fonction d'affichage du nom de la fonction
 */
void print_func_name(const char* func_name) {
   printf("[MIC-TCP] Appel de la fonction: %s\n", func_name);
}

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
// TODO: à compléter, surtout les vérifications sur l'adresse IP
//TODO: exemple || addr.ip.addr_size == 0
int verif_address(mic_tcp_sock_addr addr) {
   if(addr.port <= 1024) return -1;
   return 0;
}

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm){
   int result = -1;
   print_func_name(__FUNCTION__);
   result = initialize_components(sm); /* Appel obligatoire */
   set_loss_rate(0); /* On initialise le taux de perte à 0% */
   
   // Verifie si la liste de sockets est pleine
   if (last_used_socket > MAX_SOCKETS) return -1;
   
   last_used_socket++;
   socket_list[last_used_socket].fd = last_used_socket;
   socket_list[last_used_socket].state = CLOSED;
   
   // Retourne le descripteur du socket ou -1 en cas d'erreur
   return result == -1 ? result : last_used_socket;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr) {
   print_func_name(__FUNCTION__);
   
   //Vérifie si le socket est valide et si l'adresse est valide
   if (verif_socket(socket) == 0 && verif_address(addr) == 0) {
      // Attribue addr au socket
      socket_list[socket].local_addr = addr; /* On attribue l'adresse au socket */
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
   //en mode sans connexion on return juste 0 car pas de fiabilité
   return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr) {
   print_func_name(__FUNCTION__);
   
   // Vérifie si le socket est valide et si l'adresse est valide
   if (verif_socket(socket) == 0 && verif_address(addr) == 0) {
      socket_list[socket].remote_addr = addr; /* On attribue l'adresse distante au socket (port aussi) */
      socket_list[socket].state = ESTABLISHED; /* On change l'état du socket */
      return 0;
   }
   return -1;
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
   mic_tcp_ip_addr local_addr_ack, remote_addr_ack; // Variables pour stocker les adresses IP locales et distantes du PDU ACK

   //! Remplissage du PDU HEADER
   //mettre le numero de port local source associé a mon_socket
   pdu.header.source_port = socket_list[mic_sock].local_addr.port;
   //mettre le numero de port distant destination associé a mon_socket
   pdu.header.dest_port = socket_list[mic_sock].remote_addr.port;
   // Pour le moment on ne gère pas les numéros de séquence et d'acquittement
   pdu.header.syn = 0;
   pdu.header.ack = 0;
   pdu.header.fin = 0;
   //! Remplissage du numéro de séquence
   pdu.header.seq_num = next_sequence[mic_sock]; // Numéro de séquence du PDU
   //! Remplissage du PDU PAYLOAD
   pdu.payload.data = mesg; // On met le message dans le payload
   pdu.payload.size = mesg_size; // On met la taille du message dans le payload
   
   int ack_received = 0;
   while (ack_received == 0) {
      //? Envoi du PDU sur la couche IP 
      effective_ip_send = IP_send(pdu, socket_list[mic_sock].remote_addr.ip_addr); // On envoie le PDU sur la couche IP
      if (effective_ip_send == -1) return -1; // Erreur lors de l'envoi du PDU

      //? Attente d'un ACK avec timeout
      int recv_status = IP_recv(&pdu_ack, &local_addr_ack, &remote_addr_ack, MAX_TIMEOUT); // On attend le PDU ACK sur la couche IP
      
      //? On vérifie si le PDU_ACK reçu à un numéro de séquence valide
      if (recv_status != -1 
         && pdu_ack.header.ack == 1
         && pdu_ack.header.seq_num == next_sequence[mic_sock]+1)
      {
         ack_received = 1; // On a reçu un ACK valide
         next_sequence[mic_sock]++; // On incrémente le numéro de séquence du prochain PDU à émettre
      }
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
   }
   last_used_socket--;

   // Pour le moment on ne gère pas la fermeture de la connexion
   return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr) {
   print_func_name(__FUNCTION__);

   // Vérifie si le PDU était destiné à un de nos sockets
   int found = -1;
   for(int i = 0; i <= last_used_socket; i++) {
      if (socket_list[i].addr.port == pdu.header.dest_port) {
         found = i; // On a trouvé le socket correspondant
         break;
      }
   }
   if (found == -1) {
      printf("[MIC-TCP] PDU non destiné à un de nos sockets\n");
      return; //on ne fait rien si le PDU n'est pas pour nous
   }

   //! Verifier le num de sequence du PDU
   if (pdu.header.seq_num == next_sequence[found]) {
      // On met le PDU dans le buffer de réception du socket
      app_buffer_put(pdu.payload);
      next_sequence[found]++; // On incrémente le numéro de séquence du prochain PDU à émettre
   }
   
   //! Envoyer un acquittement ACK
   mic_tcp_pdu pdu_ack;
   // On inverse les ports source et destination
   pdu_ack.header.src_port = pdu.header.dest_port;
   pdu_ack.header.dest_port = pdu.header.src_port;
   pdu_ack.header.seq_num = next_sequence[found]; // Numéro de séquence du PDU
   pdu_ack.header.ack = 1; // On met le bit ACK à 1
   pdu_ack.header.syn = 0;
   pdu_ack.header.fin = 0;
   IP_send(pdu_ack, remote_addr); // On envoie le PDU ACK sur la couche IP
}
