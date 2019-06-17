# How to use 'core-tests' framework

## Steps needed to create another test case;

1. create a struct as a subclass `test_chain_unit_base` class, implement it in "my_test.h|cpp" files,
  e.g.:

    ```
    struct gen_rta_tests : public test_chain_unit_base
    {
      gen_rta_tests();

      //   test generator method: here we define the test sequence
      bool generate(std::vector<test_event_entry>& events) const;


      bool check1(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
      bool check_stake_registered(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>&  events);
    };
    ```
    

    1.1. implement test sequence in `bool generate(std::vector<test_event_entry>& events) const;` method;  
    
    1.2. "check" functions should follow the interface:  
    ```
    bool check_function(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>&  events);
    ```

    1.3. register "check" functions with  `REGISTER_CALLBACK_METHOD` macro invoked in constructor:
    ```
    REGISTER_CALLBACK_METHOD(gen_rta_tests, check_function);
    ```


2. add 
  
    ``` 
    #include "your_test.h" to the "chaingen_test_list.h" 
    ```

3. add 

    ```   
    GENERATE_AND_PLAY(gen_rta_tests);
    ```

    to the `chaingen_main.cpp` inside 

    ```
    else if (command_line::get_arg(vm, arg_generate_and_play_test_data))
    {
    ```
  
    block


4. (optional) define a `gen_test_options` struct representing testing hardfork table, which will be passed to cryptonote::core instance, e.g.:

    ```
    template<>
    struct get_test_options<gen_rta_tests> {
      const std::pair<uint8_t, uint64_t> hard_forks[4] = {std::make_pair(1, 0), std::make_pair(13, 14), std::make_pair(14, 73)};
        const cryptonote::test_options test_options = {
            hard_forks
        };
    };
    ```


5. implement `bool generate(std::vector<test_event_entry>& events) const;` method where you place your test     flow. Your job is to generate blocks and transactions which will be played through 'cryptonote::core' instance;
Blocks and transactions to be added to the `events` vector. Normally it added by macros so you don't have to add it manually.
  
   5.1. use `MAKE_GENESIS_BLOCK` macro to make a genesis block and add it the chain  
   5.2. use `MAKE_NEXT_BLOCK` macro to create and add block to the chain  
   5.3. use `construct_tx_with_fee` function to create transaction  
   5.4. use `MAKE_NEXT_BLOCK_TX1` macro to add transaction to a block and add block to the chain

6. schedule a "check" function call at the specific blockchain state inside `generate` method  
    ```
    DO_CALLBACK(events, "check_function");
    ```
  
