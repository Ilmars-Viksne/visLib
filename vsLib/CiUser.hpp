// This code creates a C++ class CiUser under the namespace vi. 
// The class has private variables for userId, userName, and userEmail. 
// It also includes public getterand setter methods for these variables, 
// and method printUserInfo() to print out all the user information.

#pragma once
#include <iostream>
#include <string>

namespace vi {

    class CiUser {
    private:
        int m_userId;       // User ID
        std::string m_userName;  // User Name
        std::string m_userEmail; // User Email

    public:
        // Default constructor
        CiUser() : m_userId(0), m_userName(""), m_userEmail("") {}

        // Parameterized constructor with member initializer list
        CiUser(int id, const std::string& name, const std::string& email)
            : m_userId(id), m_userName(name), m_userEmail(email) {}

        // Getter methods with 'const' to indicate they won't modify the object
        int getUserId() const { return m_userId; }
        const std::string& getUserName() const { return m_userName; }
        const std::string& getUserEmail() const { return m_userEmail; }

        // Setter methods
        void setUserId(int id) { m_userId = id; }
        void setUserName(const std::string& name) { m_userName = name; }
        void setUserEmail(const std::string& email) { m_userEmail = email; }

        // Method to print out all user information
        void printUserInfo() const {
            std::cout << "User ID: " << m_userId << '\n'
                << "User Name: " << m_userName << '\n'
                << "User Email: " << m_userEmail << '\n';
        }
    };
}
