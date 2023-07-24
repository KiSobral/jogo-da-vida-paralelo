FROM ubuntu:latest

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y python3 python3-pip build-essential && \
    apt-get install -y mpich libcurl4-openssl-dev

COPY requirements.txt /app/

WORKDIR /app

RUN pip3 install --no-cache-dir -r requirements.txt

COPY consumer.py /app/

CMD ["python3", "consumer.py"]
