# NotyHelper

## Overview
NotyHelper is a notification dispatching application designed to help users create, manage, and share predefined notifications efficiently. It provides an intuitive way for users to stay updated and communicate important messages seamlessly.

**Note:** This application is designed to work exclusively on **macOS**.

## Features
- **User Authentication**: Secure account creation and login system.
- **Predefined Notifications**: Use built-in templates for quick message dispatch.
- **Custom Notifications**: Create personalized notifications for different scenarios.
- **Notification Sharing**: Send notifications to other users in the system.
- **Persistence**: Saves user data and notifications for future reference.

## Technologies Used
- **C++**: Core application logic and backend processing.
- **Multithreading (pthread)**: Ensures smooth execution of concurrent tasks.
- **File Handling**: Uses text files to store user data and notifications.

## Installation & Setup
1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/NotyHelper.git
   cd NotyHelper
   ```
2. Give execution permissions and run the setup script:
   ```bash
   chmod +x setup.sh
   ./setup.sh
   ```
3. Run the server:
   ```bash
   ./server
   ```
4. Run the client:
   ```bash
   ./client
   ```

## How It Works
- The **server** handles notification requests and manages data.
- The **client** allows users to interact with the system by sending and receiving notifications.
- Data is stored in `users.txt`, `notifications.txt`, and `predefined.txt` to maintain user information and messages.

## Why I Built This
This project showcases my ability to work with system-level programming in C++, implement multithreading for efficient task management, and design a simple yet functional notification system. It also demonstrates my understanding of file handling, interprocess communication, and software structuring in a practical context.

Additionally, I have incorporated concepts from my **Computer Networks** courses, such as client-server architecture, interprocess communication, and concurrent processing. I also supplemented my knowledge by learning additional techniques from various online resources, allowing me to enhance the project beyond what was covered in class.

## Future Improvements
- Implementing a database for better data management.
- Adding a GUI for a more user-friendly interface.
- Introducing mobile notifications and email alerts.

## Contact
If you're interested in my work or have any questions, feel free to reach out:
- Email: ioan.lucian.meraru@gmail.com
- LinkedIn: [https://www.linkedin.com/in/meraru-ioan-10529b302/](https://linkedin.com/in/yourprofile)
- GitHub: [https://github.com/Zamdacmalfal/](https://github.com/yourusername)

