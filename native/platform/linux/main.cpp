#include "application.hpp"
#include "../../edition.hpp"
int main(int argc, char** argv) {
    return web::linux_main(argc, argv, false, web::selected_edition());
}
