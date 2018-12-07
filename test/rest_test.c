int main(int argc, char *argv[]){
    /* initialize variables */
    char *resource_id;
    char *client_id;
    char *tenant; 
    char *required_group_id;
    char response_buf[16000];
    char code_buf[100];
    char json_buf[16000];
    char user_object_id_buf[100];
    char user_profile_buf[10000];
    char user_group_buf[10000];
    cJSON *json;
    cJSON *group_membership_value; 
    char user_code[20];
    char device_code[1000];
    char raw_group_buf[160000];
    int resp;
    /* Provide hardcoded values for testing */
    resource_id = "00000002-0000-0000-c000-000000000000";
    client_id = "7262ee1e-6f52-4855-867c-727fc64b26d5";
    tenant = "digipirates.onmicrosoft.com";
    required_group_id = "d0de9e6d-93e0-4fde-b05c-0db1d376d8a8";
    request_azure_signin_code(user_code, resource_id, client_id, tenant, device_code);
    int start, end;
    printf("user code is %s\n", user_code);
    printf("device code is %s\n", device_code);
    char key[1];
    puts("Press any key to continue...");
    getchar();
    resp = request_azure_oauth_token(device_code, resource_id, client_id, response_buf);
    int inGroup = request_azure_group_membership(response_buf, required_group_id, tenant);
    if (inGroup == 2){
        printf("\nUser is in the correct group!\n");
        return 0;
    }
    printf("User is not in the correct group...");
    return 1;
}
