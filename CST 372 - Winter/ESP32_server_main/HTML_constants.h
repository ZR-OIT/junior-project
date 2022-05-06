#ifndef HTML_FUNCTIONS_H
#define HTML_FUNCTIONS_H

//These parameters are the names of the html elements
extern const char* param_weekday = "weekday";
extern const char* param_hour_block = "hour-block";
extern const char* param_pv104_status = "pv-104-status";
extern const char* param_pv107_status = "pv-107-status";
extern const char* param_pv108_status = "pv-108-status";
extern const char* param_pv110_status = "pv-110-status";
extern const char* param_pv119_status = "pv-119-status";
extern const char* param_pv120_status = "pv-120-status";
extern const char* param_pv147_status = "pv-147-status";
extern const char* param_pv153_status = "pv-153-status";
extern const char* param_login = "login";
extern const char* param_login_username = "login-username";
extern const char* param_login_password = "login-password";
extern const char* param_logout = "logout";

extern const char* params_control_monday[80] = { "pv104-mon-8to9", "pv104-mon-9to10", "pv104-mon-10to11", "pv104-mon-11to12", "pv104-mon-12to1", "pv104-mon-1to2", "pv104-mon-2to3", "pv104-mon-3to4", "pv104-mon-4to5", "pv104-mon-5to6",
                                                 "pv107-mon-8to9", "pv107-mon-9to10", "pv107-mon-10to11", "pv107-mon-11to12", "pv107-mon-12to1", "pv107-mon-1to2", "pv107-mon-2to3", "pv107-mon-3to4", "pv107-mon-4to5", "pv107-mon-5to6",
                                                 "pv108-mon-8to9", "pv108-mon-9to10", "pv108-mon-10to11", "pv108-mon-11to12", "pv108-mon-12to1", "pv108-mon-1to2", "pv108-mon-2to3", "pv108-mon-3to4", "pv108-mon-4to5", "pv108-mon-5to6",
                                                 "pv110-mon-8to9", "pv110-mon-9to10", "pv110-mon-10to11", "pv110-mon-11to12", "pv110-mon-12to1", "pv110-mon-1to2", "pv110-mon-2to3", "pv110-mon-3to4", "pv110-mon-4to5", "pv110-mon-5to6",
                                                 "pv119-mon-8to9", "pv119-mon-9to10", "pv119-mon-10to11", "pv119-mon-11to12", "pv119-mon-12to1", "pv119-mon-1to2", "pv119-mon-2to3", "pv119-mon-3to4", "pv119-mon-4to5", "pv119-mon-5to6",
                                                 "pv120-mon-8to9", "pv120-mon-9to10", "pv120-mon-10to11", "pv120-mon-11to12", "pv120-mon-12to1", "pv120-mon-1to2", "pv120-mon-2to3", "pv120-mon-3to4", "pv120-mon-4to5", "pv120-mon-5to6",
                                                 "pv147-mon-8to9", "pv147-mon-9to10", "pv147-mon-10to11", "pv147-mon-11to12", "pv147-mon-12to1", "pv147-mon-1to2", "pv147-mon-2to3", "pv147-mon-3to4", "pv147-mon-4to5", "pv147-mon-5to6",
                                                 "pv153-mon-8to9", "pv153-mon-9to10", "pv153-mon-10to11", "pv153-mon-11to12", "pv153-mon-12to1", "pv153-mon-1to2", "pv153-mon-2to3", "pv153-mon-3to4", "pv153-mon-4to5", "pv153-mon-5to6" };

extern const char* params_control_tuesday[80] = { "pv104-tues-8to9", "pv104-tues-9to10", "pv104-tues-10to11", "pv104-tues-11to12", "pv104-tues-12to1", "pv104-tues-1to2", "pv104-tues-2to3", "pv104-tues-3to4", "pv104-tues-4to5", "pv104-tues-5to6",
                                                  "pv107-tues-8to9", "pv107-tues-9to10", "pv107-tues-10to11", "pv107-tues-11to12", "pv107-tues-12to1", "pv107-tues-1to2", "pv107-tues-2to3", "pv107-tues-3to4", "pv107-tues-4to5", "pv107-tues-5to6",
                                                  "pv108-tues-8to9", "pv108-tues-9to10", "pv108-tues-10to11", "pv108-tues-11to12", "pv108-tues-12to1", "pv108-tues-1to2", "pv108-tues-2to3", "pv108-tues-3to4", "pv108-tues-4to5", "pv108-tues-5to6",
                                                  "pv110-tues-8to9", "pv110-tues-9to10", "pv110-tues-10to11", "pv110-tues-11to12", "pv110-tues-12to1", "pv110-tues-1to2", "pv110-tues-2to3", "pv110-tues-3to4", "pv110-tues-4to5", "pv110-tues-5to6",
                                                  "pv119-tues-8to9", "pv119-tues-9to10", "pv119-tues-10to11", "pv119-tues-11to12", "pv119-tues-12to1", "pv119-tues-1to2", "pv119-tues-2to3", "pv119-tues-3to4", "pv119-tues-4to5", "pv119-tues-5to6",
                                                  "pv120-tues-8to9", "pv120-tues-9to10", "pv120-tues-10to11", "pv120-tues-11to12", "pv120-tues-12to1", "pv120-tues-1to2", "pv120-tues-2to3", "pv120-tues-3to4", "pv120-tues-4to5", "pv120-tues-5to6",
                                                  "pv147-tues-8to9", "pv147-tues-9to10", "pv147-tues-10to11", "pv147-tues-11to12", "pv147-tues-12to1", "pv147-tues-1to2", "pv147-tues-2to3", "pv147-tues-3to4", "pv147-tues-4to5", "pv147-tues-5to6",
                                                  "pv153-tues-8to9", "pv153-tues-9to10", "pv153-tues-10to11", "pv153-tues-11to12", "pv153-tues-12to1", "pv153-tues-1to2", "pv153-tues-2to3", "pv153-tues-3to4", "pv153-tues-4to5", "pv153-tues-5to6" };

extern const char* params_control_wednesday[80] = { "pv104-wed-8to9", "pv104-wed-9to10", "pv104-wed-10to11", "pv104-wed-11to12", "pv104-wed-12to1", "pv104-wed-1to2", "pv104-wed-2to3", "pv104-wed-3to4", "pv104-wed-4to5", "pv104-wed-5to6",
                                                    "pv107-wed-8to9", "pv107-wed-9to10", "pv107-wed-10to11", "pv107-wed-11to12", "pv107-wed-12to1", "pv107-wed-1to2", "pv107-wed-2to3", "pv107-wed-3to4", "pv107-wed-4to5", "pv107-wed-5to6",
                                                    "pv108-wed-8to9", "pv108-wed-9to10", "pv108-wed-10to11", "pv108-wed-11to12", "pv108-wed-12to1", "pv108-wed-1to2", "pv108-wed-2to3", "pv108-wed-3to4", "pv108-wed-4to5", "pv108-wed-5to6",
                                                    "pv110-wed-8to9", "pv110-wed-9to10", "pv110-wed-10to11", "pv110-wed-11to12", "pv110-wed-12to1", "pv110-wed-1to2", "pv110-wed-2to3", "pv110-wed-3to4", "pv110-wed-4to5", "pv110-wed-5to6",
                                                    "pv119-wed-8to9", "pv119-wed-9to10", "pv119-wed-10to11", "pv119-wed-11to12", "pv119-wed-12to1", "pv119-wed-1to2", "pv119-wed-2to3", "pv119-wed-3to4", "pv119-wed-4to5", "pv119-wed-5to6",
                                                    "pv120-wed-8to9", "pv120-wed-9to10", "pv120-wed-10to11", "pv120-wed-11to12", "pv120-wed-12to1", "pv120-wed-1to2", "pv120-wed-2to3", "pv120-wed-3to4", "pv120-wed-4to5", "pv120-wed-5to6",
                                                    "pv147-wed-8to9", "pv147-wed-9to10", "pv147-wed-10to11", "pv147-wed-11to12", "pv147-wed-12to1", "pv147-wed-1to2", "pv147-wed-2to3", "pv147-wed-3to4", "pv147-wed-4to5", "pv147-wed-5to6",
                                                    "pv153-wed-8to9", "pv153-wed-9to10", "pv153-wed-10to11", "pv153-wed-11to12", "pv153-wed-12to1", "pv153-wed-1to2", "pv153-wed-2to3", "pv153-wed-3to4", "pv153-wed-4to5", "pv153-wed-5to6" };

extern const char* params_control_thursday[80] = { "pv104-thurs-8to9", "pv104-thurs-9to10", "pv104-thurs-10to11", "pv104-thurs-11to12", "pv104-thurs-12to1", "pv104-thurs-1to2", "pv104-thurs-2to3", "pv104-thurs-3to4", "pv104-thurs-4to5", "pv104-thurs-5to6",
                                                   "pv107-thurs-8to9", "pv107-thurs-9to10", "pv107-thurs-10to11", "pv107-thurs-11to12", "pv107-thurs-12to1", "pv107-thurs-1to2", "pv107-thurs-2to3", "pv107-thurs-3to4", "pv107-thurs-4to5", "pv107-thurs-5to6",
                                                   "pv108-thurs-8to9", "pv108-thurs-9to10", "pv108-thurs-10to11", "pv108-thurs-11to12", "pv108-thurs-12to1", "pv108-thurs-1to2", "pv108-thurs-2to3", "pv108-thurs-3to4", "pv108-thurs-4to5", "pv108-thurs-5to6",
                                                   "pv110-thurs-8to9", "pv110-thurs-9to10", "pv110-thurs-10to11", "pv110-thurs-11to12", "pv110-thurs-12to1", "pv110-thurs-1to2", "pv110-thurs-2to3", "pv110-thurs-3to4", "pv110-thurs-4to5", "pv110-thurs-5to6",
                                                   "pv119-thurs-8to9", "pv119-thurs-9to10", "pv119-thurs-10to11", "pv119-thurs-11to12", "pv119-thurs-12to1", "pv119-thurs-1to2", "pv119-thurs-2to3", "pv119-thurs-3to4", "pv119-thurs-4to5", "pv119-thurs-5to6",
                                                   "pv120-thurs-8to9", "pv120-thurs-9to10", "pv120-thurs-10to11", "pv120-thurs-11to12", "pv120-thurs-12to1", "pv120-thurs-1to2", "pv120-thurs-2to3", "pv120-thurs-3to4", "pv120-thurs-4to5", "pv120-thurs-5to6",
                                                   "pv147-thurs-8to9", "pv147-thurs-9to10", "pv147-thurs-10to11", "pv147-thurs-11to12", "pv147-thurs-12to1", "pv147-thurs-1to2", "pv147-thurs-2to3", "pv147-thurs-3to4", "pv147-thurs-4to5", "pv147-thurs-5to6",
                                                   "pv153-thurs-8to9", "pv153-thurs-9to10", "pv153-thurs-10to11", "pv153-thurs-11to12", "pv153-thurs-12to1", "pv153-thurs-1to2", "pv153-thurs-2to3", "pv153-thurs-3to4", "pv153-thurs-4to5", "pv153-thurs-5to6" };

extern const char* params_control_friday[80] = { "pv104-fri-8to9", "pv104-fri-9to10", "pv104-fri-10to11", "pv104-fri-11to12", "pv104-fri-12to1", "pv104-fri-1to2", "pv104-fri-2to3", "pv104-fri-3to4", "pv104-fri-4to5", "pv104-fri-5to6",
                                                 "pv107-fri-8to9", "pv107-fri-9to10", "pv107-fri-10to11", "pv107-fri-11to12", "pv107-fri-12to1", "pv107-fri-1to2", "pv107-fri-2to3", "pv107-fri-3to4", "pv107-fri-4to5", "pv107-fri-5to6",
                                                 "pv108-fri-8to9", "pv108-fri-9to10", "pv108-fri-10to11", "pv108-fri-11to12", "pv108-fri-12to1", "pv108-fri-1to2", "pv108-fri-2to3", "pv108-fri-3to4", "pv108-fri-4to5", "pv108-fri-5to6",
                                                 "pv110-fri-8to9", "pv110-fri-9to10", "pv110-fri-10to11", "pv110-fri-11to12", "pv110-fri-12to1", "pv110-fri-1to2", "pv110-fri-2to3", "pv110-fri-3to4", "pv110-fri-4to5", "pv110-fri-5to6",
                                                 "pv119-fri-8to9", "pv119-fri-9to10", "pv119-fri-10to11", "pv119-fri-11to12", "pv119-fri-12to1", "pv119-fri-1to2", "pv119-fri-2to3", "pv119-fri-3to4", "pv119-fri-4to5", "pv119-fri-5to6",
                                                 "pv120-fri-8to9", "pv120-fri-9to10", "pv120-fri-10to11", "pv120-fri-11to12", "pv120-fri-12to1", "pv120-fri-1to2", "pv120-fri-2to3", "pv120-fri-3to4", "pv120-fri-4to5", "pv120-fri-5to6",
                                                 "pv147-fri-8to9", "pv147-fri-9to10", "pv147-fri-10to11", "pv147-fri-11to12", "pv147-fri-12to1", "pv147-fri-1to2", "pv147-fri-2to3", "pv147-fri-3to4", "pv147-fri-4to5", "pv147-fri-5to6",
                                                 "pv153-fri-8to9", "pv153-fri-9to10", "pv153-fri-10to11", "pv153-fri-11to12", "pv153-fri-12to1", "pv153-fri-1to2", "pv153-fri-2to3", "pv153-fri-3to4", "pv153-fri-4to5", "pv153-fri-5to6" };
                                                 
//extern const char* hashed_acceptable_username = "16f78a7d6317f102bbd95fc9a4f3ff2e3249287690b8bdad6b7810f82b34ace3"; //This is the acceptable username encrypted in SHA256 - username
//extern const char* hashed_acceptable_password = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8"; //This is the acceptable password encrypted in SHA256 - password

extern const char* hashed_acceptable_username = "a9e35fa05f07f5bcea9698ef1f3acc7bb7c7514c7a8d6f916722b366047f554c"; //This is the acceptable username encrypted in SHA256 - edb
extern const char* hashed_acceptable_password = "0fcd568a5cb9bdb4677b69354b11ee415af8f784519cff3da49a26f84eaee7f2"; //This is the acceptable password encrypted in SHA256 - control

#endif HTML_FUNCTIONS_H
